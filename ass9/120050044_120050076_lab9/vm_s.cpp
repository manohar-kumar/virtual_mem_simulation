#include <iostream>
#include <stdio.h>
#include <fstream>
#include <thread>
#include <string>
#include <string.h>

#include <list>
#include <map>
#include <vector>
#include <mutex>
#include <chrono>
#include <condition_variable>
using namespace std;

#define BLOCKSIZE 256

//FIle management
struct dir_entry{
char file_or_dir;
string name;
int size;
int location;//can be a list of offsets(int) indicating block numbers
int protection;
int lock_info;

};

struct super{
	int no_blocks;
	int root_no;
	int metadata;

	super(){
		no_blocks=100;
		root_no=5;
		metadata=4;
	}
} superblock;

struct info{
	int pid;
	int pageNo;
};

int MemorySize;
int LowerThreshold;
int UpperThreshold;

info *frames;
int *LRU;
list<int> freeFrames;

mutex m,m_pfh,m_ffm;
condition_variable cv,cvfree,cv_pfhandler,cv_ffmtopfh;

struct ioTableEntry {
	int pagein_out;		//1 is page-in,2 is page-out	
	int pageNo;
	int pid;
	int frameNo;
	ioTableEntry (int i,int pn,int pron,int fran){
		pagein_out=i;
		pageNo=pn;
		pid=pron;
		frameNo=fran;
	}
};

struct pageTableEntry {
	bool valid;
	int frameNo;
//	char Prot;
	int ref; 
	bool modified;
	pageTableEntry(){
		valid=false;
		modified=false;
	}
	void print(){
		cout << valid << "\t"<<frameNo<<"\t"<< modified;
	}
	
};
struct proc{
	int pid;
	int processSize;
	
	pageTableEntry *pageTable;
	proc(int pid1,int processSize1){
		pid=pid1;
		processSize=processSize1;

		pageTable=new pageTableEntry[processSize];
		
	}
	proc(){
		
	}
};
void shift(int A[],int i,int pageNo,int size){
	if(i==-1){
		for(int k=0;k<size;k++){
			if(A[k]== -1) {
				A[k]=pageNo;
				break;
			}
		}
		return;
	}

	for(int j=i;j<size;j++){
		if(A[j]== -1) {
			A[j]=pageNo;
			break;
		}
		if(j==size-1){
			A[j]=pageNo;
			break;
		}
		else if(A[j+1]==-1) {
			A[j]=pageNo;
			break;
		}
		A[j]=A[j+1];
		
	}
	return;
}

list<ioTableEntry> io_table;
map<int,proc> proclist;


void pageIOmanager(){
	while(true){
		unique_lock<std::mutex> lk(m);
		while(io_table.empty()){
			cv.wait(lk);
			while(!io_table.empty()){
				io_table.pop_back();
				std::this_thread::sleep_for (std::chrono::seconds(1));
				cvfree.notify_all();
			}
		}
	}
}

bool func(int v,int pageNo,int pid,int frameNo ){
	for (list<ioTableEntry> ::iterator it=io_table.begin();it!=io_table.end();it++){
		if (it->pagein_out==v && it->pageNo==pageNo && it->pid==pid && it->frameNo==frameNo) return false;
	}
	return true;	
}

void free_frames_manager(){
	while(true){
		unique_lock<std::mutex> lk(m_ffm);
		cv_pfhandler.wait(lk, [&]{return freeFrames.size()<LowerThreshold;});

//keep implementing LRU until no of free frames equal to threshold

		while(freeFrames.size()<=UpperThreshold){
			info in;
			in = frames[LRU[0]];
			int pid=in.pid;
			int pageNo=in.pageNo;
			proc* temp= new proc();
			temp= &proclist[pid];
		unique_lock<mutex> lk(m); //define lock for when i/o


		if(!temp->pageTable[pageNo].modified){
			temp->pageTable[pageNo].valid=false;
			freeFrames.push_back(temp->pageTable[pageNo].frameNo);
		}
		else{
			ioTableEntry entry(2,pageNo,pid,LRU[0]);
			io_table.push_back(entry);

			cv.notify_all();
			
			
			cvfree.wait(lk,[&]{return func(2,pageNo,pid,LRU[0]);});
			temp->pageTable[pageNo].valid=false;
			freeFrames.push_back(temp->pageTable[pageNo].frameNo);

		}
		
		cout<<"Removed page "<<pageNo << "of Process "<< pid<<" from frame "<<LRU[0]<<endl;
		shift(LRU,0,pageNo,MemorySize);
		lk.unlock();
	}
	cv_ffmtopfh.notify_all();
	lk.unlock();
}

}


int page_fault_handler(int pid,int pageNo){
	proc* temp= new proc();
	temp= &proclist[pid];
	unique_lock<mutex> lk(m_pfh);
	if (freeFrames.size()<LowerThreshold){
		cv_pfhandler.notify_all();
		
		cv_ffmtopfh.wait(lk,[]{return freeFrames.size()>=LowerThreshold;});
		lk.unlock();
	}
	if(freeFrames.size() !=0){
		
		//int k=temp->LRU[0];
		//temp->pageTable[k].valid=false;
		//temp->frames[temp->pageTable[k].frameNo]=pageNo;
		
		temp->pageTable[pageNo].valid=true;
		temp->pageTable[pageNo].frameNo=freeFrames.front();
		freeFrames.pop_front();
		
		//cout<<"Removed page "<<k<<" from frame "<<temp->pageTable[k].frameNo<<endl;
		info in;
		in.pid=pid;
		in.pageNo=pageNo;
		frames[temp->pageTable[pageNo].frameNo]=in;
		ioTableEntry entry(1,pageNo,pid,temp->pageTable[pageNo].frameNo);
		io_table.push_back(entry);
		
		cv.notify_all();
		//unique_lock<mutex> lk(m_pfh);

		cvfree.wait(lk,[&]{return func(1,pageNo,pid,temp->pageTable[pageNo].frameNo);});
		
		
		cout << "Loaded page "<<pageNo <<"of Process "<< pid <<" into frame "<<temp->pageTable[pageNo].frameNo<< endl;
		shift(LRU,-1,pageNo,MemorySize);
		//cout << pageNo<<endl;
		lk.unlock();
		return temp->pageTable[pageNo].frameNo;
	}
	
}

int mmu(int pid, int pageNo){
	proc* temp=new proc();
	temp= &proclist[pid];
	
	if(pageNo>=temp->processSize){
		return 0;
	}
	if(!temp->pageTable[pageNo].valid) {
		return -1;
	}
	else {
		for(int i=0;i<MemorySize;i++){
			if(LRU[i]==temp->pageTable[pageNo].frameNo) {
				shift(LRU,i,temp->pageTable[pageNo].frameNo,MemorySize);
				break;
			}
		}
		return 1;
	}
}
void parse(string A[],string line){
	int start=0,no=0,i=0;
	for (i=0; i<line.length(); ++i){
		if(line[i]==' ' || line[i]==',') {
			no++;
			A[no-1]=line.substr(start,i-start);
			//cout<<A[no-1]<<endl;
			start=i+1;
		}
	}
	no++;
	A[no-1]=line.substr(start,i-start);
	//cout<<A[no-1]<<endl;
	return;
}

void process(int pid,int processSize){
	string s="s";
	string line;
	proc* temp=new proc();
	temp= &proclist[pid];
	//cout<<"adsff"<<endl;
	s=s + to_string(pid);
	ifstream spec (s);
	int access=0,modify=0,pageFault=0;
	
	if (!spec.is_open()) {
		cout <<"Error opening file";
		return;
	}

	else{
		while ( getline (spec,line) ){
			string commands[3];
			parse(commands,line);
			if(commands[0]=="end"){
				cout<<"Number of access operations : "<<access<<endl;
				cout<<"Number of modify operations : "<<modify<<endl;
				cout<<"Number of page faults: "<<pageFault<<endl;
				return;
			}
			if(commands[0]=="access") {
				int pageNo,wordNo;
				pageNo= stoi(commands[1]);
				wordNo= stoi(commands[2]);
				int status=mmu(pid,pageNo);
				cout <<"process number: "<<pid <<" attempted to access "<<pageNo<<" "<<wordNo<<endl;
				if(status==1){
					access++;
				}
				if(status==0){
					cout<<"Reported a memory protection violation"<<endl;
				}
				if(status==-1){
					pageFault++;
					
					cout<<"Reported a page fault"<<endl;
					page_fault_handler(pid,pageNo);
				}
				cout<<"Accessed page frame number "<<temp->pageTable[pageNo].frameNo<<endl;
			}
			else if(commands[0]=="modify") {
				int pageNo,wordNo;
				pageNo= stoi(commands[1]);
				wordNo= stoi(commands[2]);
				int status=mmu(pid,pageNo);
				cout <<"process number: "<<pid <<" attempted to modify "<<pageNo<<" "<<wordNo<<endl;
				if(status==1){
					modify++;
					temp->pageTable[pageNo].modified=true;
				}
				if(status==0){
					cout<<"Reported a memory protection violation"<<endl;
				}
				if(status==-1){
					pageFault++;
					
					cout<<"Reported a page fault"<<endl;
					page_fault_handler(pid,pageNo);
				}
				cout<<"Accessed page frame number "<<temp->pageTable[pageNo].frameNo<<endl;
			}
			else{
				cout<<"command not found";
				return;
			}
		}
		
	}	
}

int main() {
	string line;
	vector<thread> v;
	FILE* f=fopen("test","w+");
	char* first_block=new char[BLOCKSIZE];

	memcpy(first_block,&superblock,sizeof(superblock));
	for (int i=0;i<BLOCKSIZE;i++){
		fputc(first_block[i],f);
	}
	fseek(f,0,SEEK_SET);
	for (int i=0;i<BLOCKSIZE;i++){
		cout<<fgetc(f);
	}
	cout<<endl;
	cin>>line;
	//cout<<"adgaga"<<endl;
	thread PageIOmanager(pageIOmanager);
	thread FreeFramemanager(free_frames_manager);

	//cout<<"adgaga"<<endl;
	ifstream init ("init");
	if (!init.is_open()) {
		cout <<"Error opening file";
		return -1;
	}

	else {
		while ( getline (init,line) ){
			string commands[3];
			//cout<<"aef"<<endl;
			parse(commands,line);
			//cout<<"afdggaaef"<<endl;
			if(commands[0]=="Memory_size") {
				MemorySize=stoi(commands[1]);
				frames=new info[MemorySize];
				LRU=new int[MemorySize];
				for(int i=0;i<MemorySize;i++){
					freeFrames.push_back(i);
					LRU[i]=-1;
				}
			}
			else if(commands[0]=="Lower_threshold") {
				LowerThreshold=stoi(commands[1]);
			}
			else if(commands[0]=="Upper_threshold") {
				UpperThreshold=stoi(commands[1]);
			}
			else if(commands[0]=="Create") {
				
				int pid,processSize;
				pid= stoi(commands[1]);
				processSize= stoi(commands[2]);

				proc p(pid,processSize);

				proclist[pid]=p;

			//	thread Process(process,pid,processSize);
				v.push_back(thread(process,pid,processSize));
				//Process.join();
			}
			else if(commands[0]=="Page_table") {
				for (map<int,proc>::iterator it=proclist.begin(); it!=proclist.end(); ++it){
					cout << "Page Table of "<<it->first<<" :"<<endl;
					cout <<"no\tvalid\tframeNo\tmodified\n";
					for(int i=0;i<it->second.processSize;i++){
						cout << i<< "\t";
						(it->second).pageTable[i].print() ;
						cout<<endl;
					}
					cout <<endl;
				}
			}
			else{
				cout<<"command not found";
				return -1;
			}
		}

		
	}
	PageIOmanager.detach();
	FreeFramemanager.detach();
	for(int i=0;i<v.size();i++){
		v[i].join();
	}
}
