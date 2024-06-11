#include "squeeze/wrap/file_squeeze.h"

namespace squeeze::wrap {

void FileSqueeze::update()
{
    clear_appendee_pathset();
    squeeze.update();
}

}
