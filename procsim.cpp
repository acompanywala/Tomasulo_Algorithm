#include "procsim.hpp"
#include <string>
#include <iostream>
#include "sstream"
#include "fstream"
#include <stdint.h> 
#include<math.h>
#include<vector>
#include <cstdio>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
using namespace std;

uint64_t f0,f1,f2,N,b;
proc_inst_t inst_ptr;                        	// Object of proc_inst_t
bool instr_valid;				// To check if instruction is valid
int schd_queue_size;				// size of schedule queue
uint32_t dispatch_count_initial=0;		// start of dispatch queue
uint32_t dispatch_count_final=0;		// end of dispatch queue 
uint32_t max_dq_size=0;				// max dispatch queue size
uint32_t temp_dq_size=0;     			// temp variable to calculate avg dispatch queue size
uint32_t inst_tag_count=0;			// instruction count	
uint32_t retired_count=0;			
uint32_t fetched_count=0;
uint32_t cycle_count=0;
int32_t fire_count=0;
int32_t avg_inst_fired=0;
uint64_t dispatch_queue_size=0;
int32_t fetch=0;
int32_t zero=0;					// to initialize 2D statistics vector
int32_t fetch_stats=0;				

vector<vector<int32_t>>instructions;		// 2D vector for dispatch queue
vector<int32_t>data;

vector<vector<int32_t>>statistics;		// 2D vector for statictics			
vector<int32_t>stats;

struct queue  					// struct for schedule queue
{
  	uint32_t inst_tag;			// instruction tag 
  	int fu;					// FU # required
	uint32_t dest_reg;			// Dest reg #
  	uint32_t dest_reg_tag;			// Dest reg tag
  	uint32_t src1_reg_ready;			// Src 1 ready bit
  	uint32_t src1_reg_tag;			// Src 1 tag
  	uint32_t src2_reg_ready;			// Src 2 ready bit
  	uint32_t src2_reg_tag;			// Src 2 tag
  	bool issue_bit;				// fire bit
  	bool complete_bit;			// execution complete bit
  	bool delete_bit;			// status update complete bit...delete instruction from schedule queue 
  	uint32_t exe_counter;			// counter for execution priority
  	bool empty;				// empty bit to check is schedule queue is empty
  	bool schd_tag;				// schd tag to differentiate instruction with lowest tag that is ready to fire but FU 
};						// unavailable 
queue schd_queue[50];				// Object of queue	

struct file					// struct for register file
{
	bool ready;
  	uint32_t tag;
};
file reg_file[128];				// Object of file
  
struct scoreboard				// struct for scoreboard	
{
	  bool busy;
};
scoreboard fu0[10];				// Object of scoreboard for FU0
scoreboard fu1[10];				// Object of scoreboard for FU1
scoreboard fu2[10];				// Object of scoreboard for FU2
  
struct bus 					// struct for CDB bus					
{
	  bool busy;
	  uint32_t reg;
	  uint32_t tag;
};
bus cdb_bus[30];				// Object of bus
			
void iFetch(proc_inst_t* inst_ptr1); 		// Function to fetch instructions
void iDispatch();				// Function to dispatch instructions	
void iSchedule();				// Function to schedule instructions
void iExecute();				// Function to execute instructions
void iStatus_Update();				// Function to perform status update

void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f) 
{
	N=f;					// Fetch rate
	f0=k0;					// # FU 0
	f1=k1;					// # FU 1
	f2=k2;					// # FU 2
	b=r;					// # Result Buses

	schd_queue_size=(f0+f1+f2)*2;

	for(int i = 0; i < schd_queue_size; i++)
  	{
  		memset(&schd_queue[i], 0, sizeof(queue));	// initialize scheduling queue to zero
  	}

  	for(int i=0; i<128; i++)
  	{
  		reg_file[i].ready=1;				// initialize all registers to ready
  		reg_file[i].tag=0;				// initialize all register tags to 0
  	}

  	for(uint64_t i=0; i<f0; i++)
  	{
  		fu0[i].busy=0;					// initialize FU 0 to ready
  	}
  
  	for(uint64_t i=0; i<f1; i++)
  	{
  		fu1[i].busy=0;					// initialize FU 1 to ready
  	}
  
  	for(uint64_t i=0; i<f2; i++)
  	{
  		fu2[i].busy=0;					// initialize FU 2 to ready
  	}

	for(uint64_t i=0; i<b; i++)
	{
		cdb_bus[i].busy=0;				// initialize CDB to ready
		cdb_bus[i].reg=0;				// initialize CDB reg to zero
		cdb_bus[i].tag=0;				// initialize CDB tag to zero 
	}
	
	stats.push_back(zero);					// push back to initialize statistics vector
	stats.push_back(zero);					// push back to initialize statistics vector
	stats.push_back(zero);					// push back to initialize statistics vector
	stats.push_back(zero);					// push back to initialize statistics vector
	stats.push_back(zero);					// push back to initialize statistics vector
	stats.push_back(zero);					// push back to initialize statistics vector
}

void run_proc(proc_stats_t* p_stats)
{
	do
	{
		iStatus_Update();				// Call Status Update
		iExecute();					// Call Execute
		iSchedule();					// Call Schedule
		iDispatch();					// Call Dispatch
		iFetch(&inst_ptr);				// Call Fetch
		cycle_count++;					// increment cycle count
		temp_dq_size=(dispatch_count_final-dispatch_count_initial);
		dispatch_queue_size += temp_dq_size;		// calculate cumulative dispatch queue size

		if(temp_dq_size>max_dq_size)			// Find the maximum dispatch queue size
		{
			max_dq_size = temp_dq_size;
		}
	}
	
	while(retired_count<fetched_count);			
	
	instructions.clear();					// clear the dispatch queue
	instructions.shrink_to_fit();				// shrink the dispatch queue
	avg_inst_fired=(fire_count/cycle_count);		// calculate avgerage instructions fired
	
	printf ("INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE\n");	// print statictics
	for(uint32_t i=0; i<dispatch_count_final; i++)		// print statictics
	{
		cout<<statistics[i][0]<<"\t"<<statistics[i][1]<<"\t"<<statistics[i][2]<<"\t"<<statistics[i][3]<<"\t"<<statistics[i][4]<<"\t"<<statistics[i][5]<<endl;
	}
	cout<<endl; 	
}

 
void complete_proc(proc_stats_t *p_stats) 
{
	p_stats->avg_inst_fired = ((double)fire_count/(double)cycle_count);
	p_stats->retired_instruction = (double)dispatch_count_final;
	p_stats->avg_disp_size=((double)dispatch_queue_size/(double)cycle_count);
	p_stats->cycle_count=(double)cycle_count;
	p_stats->max_disp_size = (double)max_dq_size;
	p_stats->avg_inst_retired = ((double)retired_count/(double)cycle_count);
}


void iFetch(proc_inst_t* inst_ptr1)
{
	fetch++;						// increment fetch counter
	for(uint64_t i=0; i<N; i++)					// fetch the instructions into the dispatch queue
	{
		instr_valid = read_instruction(inst_ptr1);	// check if its a valid instruction
		
		if(instr_valid)					// put instruction in dispatch queue if valid
		{
			data.push_back(inst_ptr1->op_code);
			data.push_back(inst_ptr1->dest_reg);
			data.push_back(inst_ptr1->src_reg[0]);
			data.push_back(inst_ptr1->src_reg[1]);
			instructions.push_back(data);
			statistics.push_back(stats);		
			data.clear();
			dispatch_count_final++;
			fetched_count++;
			statistics[fetch_stats][1]=fetch;		// update statistics
			statistics[fetch_stats][2]=fetch+1;		// update statistics
			statistics[fetch_stats][0]=fetch_stats+1;	// update statistics
			fetch_stats++;
                }
                
        }
	
	for(int j=0; j<schd_queue_size; j++)	                        // Delete instruction from schedule queue if delete bit is set
	{		
		if(schd_queue[j].delete_bit == 1)
		{
			memset(&schd_queue[j], 0, sizeof(queue));
    			retired_count++;				// increment retired count
    		}
	}
}
   
void iDispatch()
{
	if(dispatch_count_initial+schd_queue_size< dispatch_count_final) 	// if schedule queue is empty put instruction from 
	{									// dispatch queue into schedule queue
		for(uint32_t i=dispatch_count_initial; i<(dispatch_count_initial+schd_queue_size); i++)
		{
			for(int j=0; j<schd_queue_size; j++)
			{
				if(schd_queue[j].empty==0)
				{
					inst_tag_count++;
    					schd_queue[j].inst_tag = inst_tag_count;
    					schd_queue[j].fu = instructions[i][0]; 
   					schd_queue[j].dest_reg = instructions[i][1];
  					schd_queue[j].issue_bit = 0;
    					schd_queue[j].complete_bit = 0;
    					schd_queue[j].delete_bit = 0;
    					schd_queue[j].exe_counter = 0; 
    					schd_queue[j].empty = 1;
    					schd_queue[j].dest_reg_tag = schd_queue[j].inst_tag;
    					statistics[inst_tag_count-1][3]=fetch+2;
    				
    				
    					if(reg_file[instructions[i][2]].ready==1 || instructions[i][2] ==-1 )	// check if source 1 is
    					{									// ready and set ready bit
    						schd_queue[j].src1_reg_ready = 1;
    						schd_queue[j].src1_reg_tag = 0;
	   				}
    					else									// else make busy
    					{
    						schd_queue[j].src1_reg_ready = 0;
    						schd_queue[j].src1_reg_tag = reg_file[instructions[i][2]].tag;
    					}
    	
    					if(reg_file[instructions[i][3]].ready==1 || instructions[i][3] ==-1)	// check if source 2 is
    					{									// ready and set ready bit
    						schd_queue[j].src2_reg_ready = 1;
    						schd_queue[j].src2_reg_tag = 0;
    					}
    					else									// else make busy
    					{
    						schd_queue[j].src2_reg_ready = 0;
    						schd_queue[j].src2_reg_tag = reg_file[instructions[i][3]].tag;
    					}
    	
    					if(instructions[i][1]!=-1)	// if destination reg is not equal to -1 then make reg busy in reg 
    					{				// file and assign tag to the register
   						reg_file[instructions[i][1]].tag=schd_queue[j].inst_tag;
    						reg_file[instructions[i][1]].ready=0;
    					}

    					dispatch_count_initial++;	// increment dispatch count initial
    					break;
 				}
			} 
		}
	}

	else		// for dispatch queue size > (dispatch count initial + schdule queue size) if schedule queue is empty put 
	{		// instruction from dispatch queue to schedule queue
		for(uint32_t i=dispatch_count_initial; i<dispatch_count_final; i++)
		{
			for(int j=0; j<schd_queue_size; j++)
			{
				if(schd_queue[j].empty==0)
				{
					inst_tag_count++;
    					schd_queue[j].inst_tag = inst_tag_count;
    					schd_queue[j].fu = instructions[i][0]; 
   					schd_queue[j].dest_reg = instructions[i][1];
  					schd_queue[j].issue_bit = 0;
    					schd_queue[j].complete_bit = 0;
    					schd_queue[j].delete_bit = 0;
    					schd_queue[j].exe_counter = 0; 
    					schd_queue[j].empty = 1;
    					schd_queue[j].dest_reg_tag = schd_queue[j].inst_tag;
    					statistics[inst_tag_count-1][3]=fetch+2;
    				
    				
    					if(reg_file[instructions[i][2]].ready==1 || instructions[i][2] ==-1 ) 	// check if source 1 is
    					{									// ready and set ready bit
    						schd_queue[j].src1_reg_ready = 1;
    						schd_queue[j].src1_reg_tag = 0;
   					}
    					else									// else make busy
    					{
    						schd_queue[j].src1_reg_ready = 0;
    						schd_queue[j].src1_reg_tag = reg_file[instructions[i][2]].tag;
    					}
    
    					if(reg_file[instructions[i][3]].ready==1 || instructions[i][3] ==-1)	// check if source 2 is
    					{									// ready and set ready bit
    						schd_queue[j].src2_reg_ready = 1;
    						schd_queue[j].src2_reg_tag = 0;
    					}
    					else									// else make busy
    					{
    						schd_queue[j].src2_reg_ready = 0;
    						schd_queue[j].src2_reg_tag = reg_file[instructions[i][3]].tag;
    					}
    	
    					if(instructions[i][1]!=-1)	// if destination reg is not equal to -1 then make reg busy in reg 
    					{				// file and assign tag to the register
   						reg_file[instructions[i][1]].tag=schd_queue[j].inst_tag;
    						reg_file[instructions[i][1]].ready=0;
    					}

    					dispatch_count_initial++;	// increment dispatch count initial
    					break;
 				}
			} 
		}
	}
}

void iSchedule()
{
	for(int i = 0; i < schd_queue_size; i++)	
  	{
  		schd_queue[i].schd_tag=0;		// reset schd_tag to zero
  	}

	for(int i = 0; i < schd_queue_size; i++)	// find the lowest tag
  	{
		uint32_t temp = 1000000;
		uint32_t u =0;
		
		for(int j=0;j<schd_queue_size;j++)
    		{
        		
        		if(schd_queue[j].src1_reg_ready == 1 && schd_queue[j].src2_reg_ready == 1 && schd_queue[j].inst_tag<temp && schd_queue[j].issue_bit != 1 && schd_queue[j].empty==1 && schd_queue[j].complete_bit !=1 && schd_queue[j].delete_bit !=1 && schd_queue[j].schd_tag==0)
        		{
				temp=schd_queue[j].inst_tag;
				u=j;
			}
    		
		}
  		
  		if (schd_queue[u].src1_reg_ready == 1 && schd_queue[u].src2_reg_ready == 1 && schd_queue[u].empty==1 && schd_queue[u].complete_bit !=1 && schd_queue[u].delete_bit !=1 && schd_queue[u].issue_bit !=1  )   	// if both sources are ready check if FU 
  		{										// is available
  			if(schd_queue[u].fu == 0)
  			{
  				for(uint32_t j=0; j<f0; j++)
  				{
  					if(fu0[j].busy==0)
  					{
  						schd_queue[u].issue_bit = 1;
  						fu0[j].busy=1;
  						fire_count++;
  						statistics[schd_queue[u].inst_tag-1][4]=fetch+2;
  						break;
  					}
  				}
  				schd_queue[u].schd_tag=1;
  			}
  
			else if(schd_queue[u].fu == 1 || schd_queue[u].fu == -1)
  			{
  				for(uint32_t j=0; j<f1; j++)
  				{
  					if(fu1[j].busy==0)
  					{
  						schd_queue[u].issue_bit = 1;
  						fu1[j].busy=1;
  						fire_count++;
  						statistics[schd_queue[u].inst_tag-1][4]=fetch+2;
  						break;
  					}
  					
  				}
  				schd_queue[u].schd_tag=1;
  			}
  
  			else if(schd_queue[u].fu == 2)
  			{
  				for(uint32_t j=0; j<f2; j++)
  				{
  					if(fu2[j].busy==0)
  					{
  						schd_queue[u].issue_bit = 1;
  						fu2[j].busy=1;
  						fire_count++;
  						statistics[schd_queue[u].inst_tag-1][4]=fetch+2;
  						break;
 					}
 					
  				}
  			schd_queue[u].schd_tag=1;	// make schd_tag = 1 to avoid re-searching for FU if FU is not available 
  			}
  		}
	}
}

void iExecute()
{
	for(int i=0; i<schd_queue_size; i++)		// increment execution counter
	{
		if(schd_queue[i].issue_bit ==1)
		{
			schd_queue[i].exe_counter++;
		}
	}

	for(uint32_t j=0; j<b; j++)
	{
		if(cdb_bus[j].busy==0)			// check if CDB is free
		{
			uint32_t temp = 0;
			uint32_t temp1 = 1000000;
			uint32_t u =0;
		
			for(int i=0;i<schd_queue_size;i++)	// check for largest execution counter
    			{
        			if(schd_queue[i].exe_counter>temp && schd_queue[i].issue_bit ==1 )
        			{
					temp=schd_queue[i].exe_counter;	
				}
			}
		
			for(int i=0;i<schd_queue_size;i++)	// find the lowest tag among the higest counter
    			{
    				if(schd_queue[i].exe_counter == temp && schd_queue[i].issue_bit ==1)
    				{
    					if(schd_queue[i].inst_tag<temp1)
        				{
						temp1=schd_queue[i].inst_tag;
						u=i;
					}
				}
			}
			
			if(schd_queue[u].issue_bit ==1)		// if the lowest tag instruction is ready to fire give it the CDB 
			{					
				schd_queue[u].issue_bit = 0;	
				schd_queue[u].exe_counter =0;
				cdb_bus[j].busy=1;
				cdb_bus[j].reg=schd_queue[u].dest_reg;
				cdb_bus[j].tag= schd_queue[u].dest_reg_tag;
				schd_queue[u].complete_bit =1;
				statistics[schd_queue[u].inst_tag-1][5]=fetch+2;
			}
				
			for(int m=0; m<128; m++)		// update reg file
 		 	{	
 		 		if(reg_file[m].tag==cdb_bus[j].tag)
		  		{
		 			reg_file[m].ready=1;
		  			reg_file[m].tag=0;
		  			break;
		  		}
			}		
		
			if(schd_queue[u].fu==0)			// if instruction uses FU 0 then free FU 0
			{
				for(uint32_t n=0; n<f0; n++)
 		 		{
 		 			if(fu0[n].busy==1)
 		 			{
 		 				fu0[n].busy=0;
 		 				break;
 		 			}
 				}
 			}
		
			else if(schd_queue[u].fu==1 || schd_queue[u].fu==-1)	// if instruction uses FU 1 then free FU 1
			{
				for(uint32_t n=0; n<f1; n++)
 		 		{
 		 			if(fu1[n].busy==1)
 		 			{
 		 				fu1[n].busy=0;
 		 				break;
 		 			}
 				}
 			}
 		
 			else if(schd_queue[u].fu==2)		// if instruction uses FU 2 then free FU 2
			{
				for(uint32_t n=0; n<f2; n++)
 		 		{
 		 			if(fu2[n].busy==1)
 		 			{
 		 				fu2[n].busy=0;
 		 				break;
 		 			}
 				}	
			}		
		}	
	}	
}



void iStatus_Update()
{

	for(uint32_t j=0; j<b; j++)				// update scheduling queue src regs
	{
		for(int i = 0; i < schd_queue_size; i++)
 		{
  			if(schd_queue[i].src1_reg_tag == cdb_bus[j].tag)
  			{
  				schd_queue[i].src1_reg_tag=0;
  				schd_queue[i].src1_reg_ready=1;
  			}
  
 			if(schd_queue[i].src2_reg_tag == cdb_bus[j].tag)
 			{
 				schd_queue[i].src2_reg_tag=0;
  				schd_queue[i].src2_reg_ready=1;
  			}
  		}
 	}
 
	for(int p = 0; p < schd_queue_size; p++)		// if instruction complete then set delete bit
  	{
  		if(schd_queue[p].complete_bit ==1)
  		{
  			
  			schd_queue[p].delete_bit=1;
  			schd_queue[p].complete_bit =0;

 			for(uint32_t j=0; j<b; j++)			// free CDB bus
			{
				if(schd_queue[p].inst_tag == cdb_bus[j].tag)
				{
					cdb_bus[j].busy=0;
					cdb_bus[j].tag=0;
					cdb_bus[j].reg=0;
					break;
				}
  			}
  		}
  	}
}
