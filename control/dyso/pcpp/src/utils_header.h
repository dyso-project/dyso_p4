#pragma once

/**
 * control packet's custom header fields
 * keys : for each row
 * recs : packet signatures (hashes) to collect
 */ 

namespace pcpp{
    struct dysoCtrlhdr {
        uint32_t index_update;
        uint32_t key0;
        uint32_t key1;
        uint32_t key2;
        uint32_t key3;
        
        uint32_t index_probe;
        uint32_t rec0;
        uint32_t rec1;
        uint32_t rec2;
        uint32_t rec3;
        uint32_t rec4;
        uint32_t rec5;
        uint32_t rec6;
        uint32_t rec7;
    };
}