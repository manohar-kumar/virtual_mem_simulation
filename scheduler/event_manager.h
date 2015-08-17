#include <iostream>
#include <vector>
#include <queue>


using namespace std;

struct event{
	int end_t;	//event occurrence time 
	int type;	//event type
	int pid;	//process id
};

class comp{
public:
 	int operator() ( const event& p1, const event &p2)
 	{
 		return p1.end_t>p2.end_t;
 	}
};

class event_mgnr {
	public:
		priority_queue<event, vector<event>, comp> event_table;

	//function for adding an event in the event table
	
	/*event_mngr operator=(event_mngr& b)
      {
         Box box;
         box.length = this->length + b.length;
         box.breadth = this->breadth + b.breadth;
         box.height = this->height + b.height;
         return box;
      }
      * */

	void add_event(int end_t, int type, int pid)
	{
		event ev;
		ev.end_t = end_t;
		ev.type = type;
		ev.pid = pid;
		event_table.push(ev);
	}

	//Is event table empty..?
	bool is_empty()
	{
		return event_table.empty();
	}
	
	//function for returning the top_most entry of the event table
	event next_event()
	{
		event ev = event_table.top();
		event_table.pop();
		return ev;
	}

};
