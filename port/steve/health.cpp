#include "native.h"
extern "C" {extern short data_02092144[];
void GiveHealth(int index,int amount) {
    if(steve::health_hook && steve::health_hook(index,amount)) return;
    // The original hosted seat's 8.8 health arithmetic and bounds.
    data_02092144[index]+=amount;
    if(data_02092144[index]>0x880) {data_02092144[index]=0x880;return;}
    if(data_02092144[index]<0) data_02092144[index]=0;
}}
