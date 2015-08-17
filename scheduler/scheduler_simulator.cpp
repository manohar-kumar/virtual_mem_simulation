#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include "event_manager.h"

using namespace std;

event_mgnr em;
int running_prior;
struct scheduling_level {
	int level_no;
	int priority;
	int time_slice;	//either some integer or N
};

struct scheduler {
	int no_of_levels;
	vector<scheduling_level> levels_arr;
};

struct process_phase {
	int itrs;	//number of iterations
	int cpu_b;	//cpu burst time
	int io_b;	//IO burst time
	int cpu_temp;
	int io_temp;
};

struct process {
	int pid;
	int start_priority;
	int admission;
	vector<process_phase> phases;
};

scheduler Scheduler;
vector<process> p_list;

void handling_PROCESS_SPEC_file(){
	string line, line2;
	int pid, prior;
	int adm;
	int iter;
	int cpu_t, io_t;
	ifstream infile("PROCESS_SPEC");
	while (std::getline(infile, line))
	{
		if(line=="PROCESS"){
			process proc;
			getline(infile, line2);
			std::istringstream iss(line2);
		        if (!(iss >> pid >> prior >> adm)) { break; } // error
		    
			proc.pid = pid;
			proc.start_priority = prior;
			proc.admission = adm;

			getline(infile, line2);
			while(line2 != "END"){
				std::istringstream iss(line2);
				process_phase pp;
			        if (!(iss >> iter >> cpu_t >> io_t)) { break; } // error
			    
				pp.itrs = iter;
			    	pp.cpu_b = cpu_t;
			    	pp.cpu_temp=cpu_t;
			    	pp.io_b = io_t;
			    	pp.io_temp=io_t;
			    	(proc.phases).push_back(pp);
			    	getline(infile, line2);
			}
			p_list.push_back(proc);
			em.add_event(proc.admission,1,proc.pid);	//event type "1" represents "process admission event"

		}
	}
}

int string_to_integer(string str)
{
	int r=1,s=0,l=str.length(),i;
	for(i=l-1;i>=0;i--)
	{
		s = s + ((str[i] - '0')*r);
		r *= 10;
	}
	return s;
}

void handling_SCHEDULER_SPEC_file(){
	string line, line2;
	int level_count;
	int prior;
	int s_lvl;
	int t_slice;
	string t_slice1;
	ifstream infile("SCHEDULER_SPEC");
	while (std::getline(infile, line))
	{
		if(line=="SCHEDULER"){
			getline(infile, line2);
			std::istringstream iss(line2);
		    if (!(iss >> level_count)) { break; } // error
			
			Scheduler.no_of_levels = level_count;
			for(int i=0; i<level_count; i++){
				getline(infile, line2);
				std::istringstream iss(line2);
				if (!(iss >> s_lvl >> prior >> t_slice1)) { break; } // error
				scheduling_level scl;
				if(t_slice1 == "N")
					t_slice = 9999;
				else
					t_slice = string_to_integer(t_slice1);
				scl.level_no = s_lvl;
				scl.priority = prior;
				scl.time_slice = t_slice;
				
				Scheduler.levels_arr.push_back(scl);
			}
		}
	}
}
process* prio_ret(int piid){
	for (int i=0;i<p_list.size();i++){
		if (piid==p_list[i].pid) return &p_list[i];
	}
}
int main()
{
	event start_io;
	handling_PROCESS_SPEC_file();
	handling_SCHEDULER_SPEC_file();
	//processing events
	event last_event;
	last_event.pid=-1;
	int check = 0;
	event next;
	int max_prio = 0;
	int max_prio_pid;
	int pi;
	priority_queue<event, vector<event>, comp> new_table;
	vector <int> io_running_pid;
	while(!em.is_empty()){
		next = em.next_event();
		if (last_event.pid!=-1){pi=last_event.pid;}
		else pi=0;
	switch(next.type)
	{	
		//routine for handling process admission event
		case 1: 
			cout << "PID ::" <<next.pid << " TIME ::" << next.end_t<< " EVENT :: Process Admitted"<<endl;
			if (pi!=0)
				{
				if ((*prio_ret(pi)).start_priority<(*prio_ret(next.pid)).start_priority){
				int k=(*prio_ret(last_event.pid)).phases[0].cpu_temp;
				k-=(next.end_t-last_event.end_t);
				(*prio_ret(last_event.pid)).phases[0].cpu_temp=k;
				start_io.pid=next.pid;
				start_io.type=2;
				start_io.end_t=next.end_t+(*prio_ret(next.pid)).phases[0].cpu_b;
				
				while(!em.is_empty()){
					event ev = em.event_table.top();
					em.event_table.pop();
					if (ev.type!=2){
						new_table.push(ev);
					}
				}
				while(!new_table.empty()){
					event ev = new_table.top();
					new_table.pop();
					em.event_table.push(ev);
				}
				em.event_table.push(start_io);	
				cout << "PID ::" <<pi << " TIME ::" << next.end_t<< " EVENT :: Process Pre-empted"<<endl;
				cout << "PID ::" <<next.pid << " TIME ::" << next.end_t<< " EVENT :: CPU started"<<endl;
			}
			else {}
			}
		else  {
			start_io.pid=next.pid;
			start_io.type=2;
			start_io.end_t=next.end_t+(*prio_ret(next.pid)).phases[0].cpu_temp;
	
				em.event_table.push(start_io);
			cout << "PID ::" <<next.pid << " TIME ::" << next.end_t<< " EVENT :: CPU started"<<endl;
		}
			
			break;
		case 2:   //IO Start
			
			while(!em.is_empty()){
					event ev = em.event_table.top();
					em.event_table.pop();
					new_table.push(ev);
					if (ev.type==3){
						io_running_pid.push_back(ev.pid);
					}
				}
				io_running_pid.push_back(next.pid);
				while(!new_table.empty()){
					event ev = new_table.top();
					new_table.pop();
					em.event_table.push(ev);
				}
				
				for (int j=0;j<p_list.size();j++){
					check=0;
					for (int i=0;i<io_running_pid.size();i++){
						if(p_list[j].pid== io_running_pid[i]){
							check = 1;
							break;
						}
					}
					if(check == 0){
						if(max_prio < p_list[j].start_priority){
							max_prio = p_list[j].start_priority;
							max_prio_pid = p_list[j].pid;
						}
					}
				}
			cout << "PID ::" <<next.pid << " TIME ::" << next.end_t<< " EVENT :: CPU burst completed "<<endl;
			cout << "PID ::" <<next.pid << " TIME ::" << next.end_t<< " EVENT :: IO Started"<<endl;
			em.add_event(next.end_t+(*prio_ret(next.pid)).phases[0].io_b,3,next.pid);	
			if (io_running_pid.size()!=p_list.size()) {
				em.add_event(next.end_t,4,max_prio_pid);
				em.add_event((*prio_ret(max_prio_pid)).phases[0].cpu_temp+next.end_t,2,max_prio_pid);
			}
			io_running_pid.erase(io_running_pid.begin(),io_running_pid.end());
			break;
			case 3:  //end io
			(*prio_ret(next.pid)).phases[0].itrs-=1;
			cout << "PID ::" <<next.pid << " TIME ::" << next.end_t<< " EVENT :: IO burst completed"<<endl;
			if((*prio_ret(next.pid)).phases[0].itrs > 0){
				(*prio_ret(next.pid)).phases[0].cpu_temp=(*prio_ret(next.pid)).phases[0].cpu_b;
				(*prio_ret(next.pid)).phases[0].io_temp=(*prio_ret(next.pid)).phases[0].io_b;
				if ((*prio_ret(pi)).start_priority<(*prio_ret(next.pid)).start_priority){
					(*prio_ret(last_event.pid)).phases[0].cpu_temp-=(next.end_t-last_event.end_t);
					start_io.pid=next.pid;
					start_io.type=2;
					start_io.end_t=next.end_t+(*prio_ret(next.pid)).phases[0].cpu_temp;
					while(!em.is_empty()){
						event ev = em.event_table.top();
						em.event_table.pop();
						if (ev.type!=2){
							new_table.push(ev);
						}
					}
					while(!new_table.empty()){
						event ev = new_table.top();
						new_table.pop();
						em.event_table.push(ev);
					}
					em.event_table.push(start_io);		
					em.add_event(next.end_t,4,next.pid);
				}
				else if  ((*prio_ret(pi)).start_priority==(*prio_ret(next.pid)).start_priority){
					start_io.pid=next.pid;
					start_io.type=2;
					start_io.end_t=next.end_t+(*prio_ret(next.pid)).phases[0].cpu_temp;
					em.event_table.push(start_io);		
					em.add_event(next.end_t,4,next.pid);
				}
				else {}
			}
			else {
				(*prio_ret(next.pid)).phases.erase((*prio_ret(next.pid)).phases.begin());
				if ((*prio_ret(next.pid)).phases.empty()){
					for (int i=0;i<p_list.size();i++){
						if (next.pid==p_list[i].pid)
							p_list.erase(p_list.begin()+i);
					}
					cout << "PID ::" <<next.pid << " TIME ::" << next.end_t<< " EVENT :: Process terminated"<<endl;
				}
				else {
						(*prio_ret(next.pid)).phases[0].cpu_temp=(*prio_ret(next.pid)).phases[0].cpu_b;
						(*prio_ret(next.pid)).phases[0].io_temp=(*prio_ret(next.pid)).phases[0].io_b;
						if ((*prio_ret(pi)).start_priority<(*prio_ret(next.pid)).start_priority){
						(*prio_ret(last_event.pid)).phases[0].cpu_temp-=(next.end_t-last_event.end_t);
						start_io.pid=next.pid;
						start_io.type=2;
						start_io.end_t=next.end_t+(*prio_ret(next.pid)).phases[0].cpu_temp;
						while(!em.is_empty()){
							event ev = em.event_table.top();
							em.event_table.pop();
							if (ev.type!=2){
								new_table.push(ev);
							}
						}
						while(!new_table.empty()){
							event ev = new_table.top();
							new_table.pop();
							em.event_table.push(ev);
						}
						em.event_table.push(start_io);		
						em.add_event(next.end_t,4,next.pid);
					}
					else if  ((*prio_ret(pi)).start_priority==(*prio_ret(next.pid)).start_priority){
						start_io.pid=next.pid;
						start_io.type=2;
						start_io.end_t=next.end_t+(*prio_ret(next.pid)).phases[0].cpu_temp;
						em.event_table.push(start_io);		
						em.add_event(next.end_t,4,next.pid);
					}
					else {}
					
				}
		}
		break;
		case 4:
		cout << "PID ::" <<next.pid << " TIME ::" << next.end_t<< " EVENT :: CPU burst started"<<endl;

		//Define routines for other required events here.

	}
	last_event.end_t=next.end_t;
	last_event.pid=next.pid;
	last_event.type=next.type;
}
	
	return 0;
}
