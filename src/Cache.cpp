/**
 * Cache.h encapsulates header functionality for the Cache class.
 *
 * Created on: August 27th, 2021
 * Author: Stevan Dupor
 * Copyright (C) 2021 Stevan Dupor - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 */

#include "Cache.h"

Cache::Cache(cache_params *params, bool main_mem) {
   if(main_mem){
      this->main_memory = true;
      return;
   }

}

Cache::~Cache() {

}

void Cache::read(unsigned long &addr){

}

void Cache::write(unsigned long &addr, unsigned long &data) {

}
