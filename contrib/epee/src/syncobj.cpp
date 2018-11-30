#include "syncobj.h"
#include "misc_log_ex.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "sync"

namespace epee {

void critical_section::lock()
{
  MDEBUG("critical_section: locking: " << &m_section);
  m_section.lock();
  MDEBUG("critical_section: locked: " << &m_section);
}

void critical_section::unlock()
{
  MDEBUG("critical_section: unlocking: " << &m_section);
  m_section.unlock();
  MDEBUG("critical_section: unlocked: " << &m_section);
}

}



