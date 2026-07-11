/* Stream reader version policy. */

#include "config.h"
#include "stream_version_policy.h"

int
dwg_stream_version_is_unsupported (const Dwg_Version_Type version)
{
  switch (version)
    {
    case R_2000b:
    case R_2004a:
    case R_2004c:
    case R_2007a:
    case R_2007b:
    case R_2010b:
    case R_2013b:
    case R_2018b:
      return 1;
    default:
      return 0;
    }
}
