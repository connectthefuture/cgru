#include "afnodesrv.h"

#include "../include/afanasy.h"

#include "../libafanasy/environment.h"

#include "action.h"
#include "afcommon.h"
#include "usercontainer.h"

#define AFOUTPUT
#undef AFOUTPUT
#include "../include/macrooutput.h"
#include "../libafanasy/logger.h"

// Zero solve cycle variable in nodes is initial,
// it means that node was not solved at all.
unsigned long long AfNodeSrv::sm_solve_cycle = 1;

AfNodeSrv::AfNodeSrv( af::Node * i_node, const std::string & i_store_dir):
    m_from_store( false),
	m_stored_ok( false),
    m_solve_need(0.0),
	m_solve_cycle(0), // 0 means that it was not solved at all
    m_prev_ptr( NULL),
    m_next_ptr( NULL),
	m_node( i_node)
{
//	AFINFO("AfNodeSrv::AfNodeSrv:");
//printf("this = %p\n", (void*)(this));
//printf("m_node = %p\n", (void*)(m_node));
	if( i_store_dir.size())
	{
		m_from_store = true;
		setStoreDir( i_store_dir);
	}
}

void AfNodeSrv::v_refresh( time_t currentTime, AfContainer * pointer, MonitorContainer * monitoring)
{
   AFERRAR("AfNodeSrv::refresh: invalid call: name=\"%s\", id=%d", m_node->m_name.c_str(), m_node->m_id)
   return;
}

AfNodeSrv::~AfNodeSrv()
{
//AFINFO("AfNodeSrv::~Node():")
}

void AfNodeSrv::setZombie()
{
	m_node->m_flags = m_node->m_flags | af::Node::FZombie;

	// Delete store folder (with recursion)
	if( m_store_dir.size())
		AFCommon::QueueNodeCleanUp( this);
}

void AfNodeSrv::setStoreDir( const std::string & i_store_dir)
{
	m_store_dir = i_store_dir;
	m_store_file = m_store_dir + AFGENERAL::PATH_SEPARATOR + "data.json";

	if( false == isFromStore())
		createStoreDir();
}

bool AfNodeSrv::createStoreDir() const
{
	AFINFA("AfNodeSrv::createStoreDir: %s", m_store_dir.c_str())

	if( m_store_dir.empty())
	{
		AF_ERR << "Store folder is not set for '" << m_node->m_name << "'";
		return false;
	}

	// Try to remove previous (old node) folder:
	if( af::pathIsFolder( m_store_dir))
	{
		if( false == af::removeDir( m_store_dir))
		{
			AFCommon::QueueLogError( std::string("Unable to remove old store folder:\n") + m_store_dir);
			return false;
		}
	}

	// Make path (all needed folders):
	if( af::pathMakePath( m_store_dir) == false)
	{
		AFCommon::QueueLogError( std::string("Unable to create store folder:\n") + m_store_dir);
		return false;
	}

	return true;
}

void AfNodeSrv::store() const
{
	AFINFA("AfNodeSrv::store: %s; from store: %d:", getStoreDir().c_str(), isFromStore())
	if( m_node->getId() == 0 )
	{
		AFERRAR("AfNodeSrv::store(): '%s': zero ID.", m_node->getName().c_str())
		return;
	}
	if( m_store_dir.empty())
	{
		AFERRAR("AfNodeSrv::store(): Store forder is not set for '%s'.", m_node->getName().c_str())
		return;
	}
	std::ostringstream ostr;
	m_node->v_jsonWrite( ostr, 0);
	AFCommon::QueueFileWrite( new FileData( ostr, m_store_file));

//printf("AfNodeSrv::store: END: %s; from store: %d:", getStoreDir().c_str(), isFromStore())
}

void AfNodeSrv::action( Action & i_action)
{
	if( m_node->isLocked())
		return;

	bool valid = false;
	if( i_action.data->HasMember("operation"))
	{
		const JSON & operation = (*i_action.data)["operation"];
		if( false == operation.IsObject())
		{
			AFCommon::QueueLogError("Action \"operation\" should be an object, " + i_action.author);
			return;
		}
		const JSON & type = operation["type"];
		if( false == type.IsString())
		{
			AFCommon::QueueLogError("Action \"operation\" \"type\" should be a string, " + i_action.author);
			return;
		}
		if( strlen( type.GetString()) == 0)
		{
			AFCommon::QueueLogError("Action \"operation\" \"type\" string is empty, " + i_action.author);
			return;
		}
		valid = true;
	}

	if( i_action.data->HasMember("params"))
	{
		const JSON & params = (*i_action.data)["params"];
		if( params.IsObject())
		{
			m_node->jsonRead( params, &i_action.log, i_action.monitors);
			valid = true;
		}
		else
		{
			AFCommon::QueueLogError("Action \"params\" should be an object, " + i_action.author);
			return;
		}
	}

	if( valid == false )
	{
		AFCommon::QueueLogError("Action should have an \"operation\" or(and) \"params\" object, " + i_action.author);
		return;
	}

	v_action( i_action);

	if( i_action.log.size())
	{
		if( i_action.log[0] == '\n' )
			i_action.log[0] = ' ';
		i_action.users->logAction( i_action, m_node->m_name);
		i_action.log += std::string(" by ") + i_action.author;
		appendLog( i_action.log);
	}
	else
		i_action.users->updateTimeActivity( i_action.user_name);
}

void AfNodeSrv::v_action( Action & i_action)
{
}

void AfNodeSrv::v_priorityChanged( MonitorContainer * i_monitoring ){}

/// Solving function should be implemented in child classes (if solving needed):
RenderAf * AfNodeSrv::v_solve( std::list<RenderAf*> & i_renders_list, MonitorContainer * i_monitoring)
{
    AF_ERR << "AfNodeSrv::solve(): Not implemented: " <<  m_node->m_name.c_str();
    return NULL;
}
void AfNodeSrv::v_calcNeed()
{
    AF_ERR << "AfNodeSrv::calcNeed(): Not implememted: " << m_node->m_name.c_str();
    calcNeedResouces(-1);
}
bool AfNodeSrv::v_canRun()
{
    AF_ERR << "AfNodeSrv::canRun(): Not implememted: " << m_node->m_name.c_str();
    return false;
}
bool AfNodeSrv::v_canRunOn( RenderAf * i_render)
{
    AF_ERR << "AfNodeSrv::canRunOn(): Not implememted: " << m_node->m_name.c_str();
    return false;
}

/// Compare nodes need for solve:
bool AfNodeSrv::greaterNeed( const AfNodeSrv * i_other) const
{
   if( m_solve_need > i_other->m_solve_need )
   {
      return true;
   }
   if( m_solve_need < i_other->m_solve_need )
   {
      return false;
   }

   /// If need parameters are equal,
   /// Greater node is a node that was solved earlier
   return m_solve_cycle < i_other->m_solve_cycle;
}

bool AfNodeSrv::greaterPriorityThenOlderCreation( const AfNodeSrv * i_other) const
{
    if (m_node->m_priority != i_other->m_node->m_priority)
        return m_node->m_priority > i_other->m_node->m_priority;

    // If the priority is the same, we look for the smaller creation time
    if (m_node->getTimeCreation() != i_other->m_node->getTimeCreation())
        return m_node->getTimeCreation() < i_other->m_node->getTimeCreation();

    // If creation time is the same too (likely because this type of node does not implement getTimeCreation), use the earliest solved node
    return m_solve_cycle < i_other->m_solve_cycle;
}

/// Try so solve a Node
RenderAf * AfNodeSrv::trySolve( std::list<RenderAf*> & i_renders_list, MonitorContainer * i_monitoring)
{
    RenderAf * render = v_solve( i_renders_list, i_monitoring);

    if( NULL == render )
    {
        // Was not solved
        return NULL;
    }

    // Node solved successfully:

    // Store solve cycle
    m_solve_cycle = sm_solve_cycle;

    // Calculace new need value as node got some more resource
    // ( nodes shoud increment resource value in solve function )
    v_calcNeed();

    // Icrement solve cycle
    sm_solve_cycle++;

    // Returning that node was solved
    return render;
//printf("AfNodeSrv::setSolved: '%s': cycle = %d, need = %g\n", name.c_str(), m_solve_cycle, m_solve_need);
}

void AfNodeSrv::calcNeedResouces( int i_resourcesquantity)
{
//printf("AfNodeSrv::calcNeedResouces: '%s': resourcesquantity = %d\n", name.c_str(), i_resourcesquantity);
	m_solve_need = 0.0;

// Need calculation no need as there is no need at all for some reason.
	if( i_resourcesquantity < 0)
	{
		return;
	}

	if( false == v_canRun())
	{
		// Cannot run at all - no solving needed
		return;
	}

	// Main solving function:
	// ( each priority point gives 10% more resources )
	m_solve_need = pow( 1.1, m_node->m_priority) / (i_resourcesquantity + 1.0);
}

void AfNodeSrv::appendLog( const std::string & message)
{
	m_log.push_back( af::time2str() + " : " + message);
	while( m_log.size() > af::Environment::getAfNodeLogLinesMax() ) m_log.pop_front();
}

int AfNodeSrv::calcLogWeight() const
{
	int weight = 0;
	for( std::list<std::string>::const_iterator it = m_log.begin(); it != m_log.end(); it++)
		weight += af::weigh( *it);
    return weight;
}

af::Msg * AfNodeSrv::writeLog( bool i_binary) const
{
	if( false == i_binary )
		return af::jsonMsg( "log", m_node->m_name, m_log);

	af::Msg * msg = new af::Msg;
	msg->setStringList( m_log );
	return msg;
}

