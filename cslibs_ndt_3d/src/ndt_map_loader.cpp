#include "ndt_map_loader.h"

int main(int argc, char *argv[])
{
    ros::init(argc, argv, "NDTMapLoader");

    cslibs_ndt_3d::NDTMapLoader ml;
    ml.run();

    return 0;
}
