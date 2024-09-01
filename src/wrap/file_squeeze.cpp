#include "squeeze/wrap/file_squeeze.h"

namespace squeeze::wrap {

bool FileSqueeze::update()
{
    clear_appendee_pathset();
    return squeeze.update();
}

}
