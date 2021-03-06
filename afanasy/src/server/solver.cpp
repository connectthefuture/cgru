#include "solver.h"

#include "../include/afanasy.h"
#include "../libafanasy/environment.h"

#include "afnodesrv.h"
#include "jobcontainer.h"
#include "rendercontainer.h"
#include "usercontainer.h"
#include "monitorcontainer.h"

#define AFOUTPUT
#undef AFOUTPUT
#include "../include/macrooutput.h"
#include "../libafanasy/logger.h"

JobContainer     * Solver::ms_jobcontainer    = NULL;
RenderContainer  * Solver::ms_rendercontainer = NULL;
UserContainer    * Solver::ms_usercontainer   = NULL;
MonitorContainer * Solver::ms_monitorcontaier = NULL;

int Solver::ms_solve_cycles_limit = 100000;
int Solver::ms_awaken_renders;

Solver::Solver(
		JobContainer     * i_jobcontainer,
		RenderContainer  * i_rendercontainer,
		UserContainer    * i_usercontainer,
		MonitorContainer * i_monitorcontainer
	)
{
	ms_jobcontainer    = i_jobcontainer;
	ms_rendercontainer = i_rendercontainer;
	ms_usercontainer   = i_usercontainer;
	ms_monitorcontaier = i_monitorcontainer;
}

Solver::~Solver(){}

struct MostReadyRender : public std::binary_function <RenderAf*,RenderAf*,bool>
{
	inline bool operator()( const RenderAf * a, const RenderAf * b)
	{
		if( a->getTasksNumber() < b->getTasksNumber()) return true;
		if( a->getTasksNumber() > b->getTasksNumber()) return false;

		if( a->getCapacityFree() > b->getCapacityFree()) return true;
		if( a->getCapacityFree() < b->getCapacityFree()) return false;

		if( a->getPriority() > b->getPriority()) return true;
		if( a->getPriority() < b->getPriority()) return false;

		if( a->getTasksStartFinishTime() < b->getTasksStartFinishTime()) return true;
		if( a->getTasksStartFinishTime() > b->getTasksStartFinishTime()) return false;

		if( a->getCapacity() > b->getCapacity()) return true;
		if( a->getCapacity() < b->getCapacity()) return false;

		if( a->getMaxTasks() > b->getMaxTasks()) return true;
		if( a->getMaxTasks() < b->getMaxTasks()) return false;

		return a->getName().compare( b->getName()) < 0;
	}
};

// Functor for sorting algorithm
struct GreaterNeed : public std::binary_function<AfNodeSrv*,AfNodeSrv*,bool>
{
	inline bool operator()(const AfNodeSrv * a, const AfNodeSrv * b)
	{
		return a->greaterNeed( b);
	}
};

// Other functor for an alternative sorting algorithm
struct GreaterPriorityThenOlderCreation : public std::binary_function<AfNodeSrv*,AfNodeSrv*,bool>
{
	inline bool operator()(const AfNodeSrv * a, const AfNodeSrv * b)
	{
		return a->greaterPriorityThenOlderCreation( b);
	}
};

void Solver::solve()
{
	//
	// Jobs solving:
	//
	AF_DEBUG << "Solving jobs...";

	// Get initial solve nodes list:
	std::list<AfNodeSrv*> solve_list;
	if( af::Environment::getSolvingUseUserPriority())
		solve_list = ms_usercontainer->getNodesStdList();
	else
		solve_list = ms_jobcontainer->getNodesStdList();

//########################################

	int solve_cycle = 0;
	int tasks_solved = 0;
	ms_awaken_renders = 0;

	// Run solve cycle:
	while( solve_list.size())
	{
		// Increment cycle and check limit:
		solve_cycle++;
		if( solve_cycle > ms_solve_cycles_limit )
		{
			// This should not happen.
			// Most probably it is a bug in solving code.
			AF_WARN << "Solve reached cycles limit: " << ms_solve_cycles_limit;
			break;
		}

		// Function exits on each solve success (just 1 task solved),
		// removes nodes that was not solved from list.
		RenderAf * render = SolveList( solve_list, af::Node::SolveByPriority);
		if( render )
		{
			// Check Wake-On-LAN:
			if( render->isWOLWakeAble())
			{
				AF_DEBUG << "Solving waking up render '" << render->node()->getName() << "'.";
				render->wolWake( ms_monitorcontaier, std::string("Automatic waking by a job."));
				ms_awaken_renders++;
			}
			else
				tasks_solved++;
		}

		// Check tasks solving speed limit:
		if(( af::Environment::getSolvingTasksSpeed() >= 0 ) &&
			( tasks_solved >= af::Environment::getSolvingTasksSpeed()))
		{
			break;
		}
	}

	AF_DEBUG << "Solved " << tasks_solved << " tasks within " << solve_cycle << " cycles.";

	// This needed to set render not busy if has no tasks.
	// This prevents not to reset render busy state if it runs tasks one by one.
	RenderContainerIt rendersIt( ms_rendercontainer);
	for( RenderAf * render = rendersIt.render(); render != NULL; rendersIt.next(), render = rendersIt.render())
		render->solvingFinished();
}

RenderAf * Solver::SolveList( std::list<AfNodeSrv*> & i_list, af::Node::SolvingMethod i_method)
{
	// Remove node that need no solving at all (done, offline, ...)
	for( std::list<AfNodeSrv*>::iterator it = i_list.begin(); it != i_list.end(); )
	{
		if((*it)->v_canRun())
			it++;
		else
			it = i_list.erase( it);
	}

	// Sort list if needed.
	// ( there is not need to sort when solving user jobs by list order )
	if( i_method != af::Node::SolveByOrder )
	{
		if( af::Environment::getSolvingSimpler())
			i_list.sort( GreaterPriorityThenOlderCreation());
		else
			i_list.sort( GreaterNeed());
	}

	// Iterate solving nodes list:
	for( std::list<AfNodeSrv*>::iterator it = i_list.begin(); it != i_list.end(); )
	{
		// Get renders:
		std::list<RenderAf*> renders_list;
		RenderContainerIt rendersIt( ms_rendercontainer);
		for( RenderAf * render = rendersIt.render(); render != NULL; rendersIt.next(), render = rendersIt.render())
		{
			// Check that render is ready to run a task:
			if( false == render->isReady())
			{
				// Render is not ready, but may be we can wake it up
				if(( false == render->isWOLWakeAble()) || ( ms_awaken_renders >= af::Environment::getSolvingWakePerCycle() ))
				{
					continue; ///< - We can't
				}

				AF_DEBUG << "Sleeping render '" << render->node()->getName() << "' solved by '" << (*it)->node()->getName() << "'";
			}

			// Check that the node can run this render:
			if( false == (*it)->v_canRunOn( render))
				continue;

			renders_list.push_back( render);
		}

		// Sort renders:
		renders_list.sort( MostReadyRender());

		RenderAf * render = (*it)->trySolve( renders_list, ms_monitorcontaier);

		if( render )
			return render;

		it = i_list.erase( it);
	}

	return NULL;
}

