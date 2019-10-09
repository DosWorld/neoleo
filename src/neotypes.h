#pragma once

#include <cctype>
#include <memory>
#include <cstdint>
#include <cstdlib>
//#include <unordered_set>
#include <string>
#include <variant>
#include <vector>

#include "errors.h"
#include "numeric.h"

typedef unsigned short CELLREF;
typedef struct rng { CELLREF lr, lc, hr, hc; } rng_t;

typedef struct {}  empty_t;
typedef std::variant<num_t, std::string, err_t, rng_t, empty_t> value_t;
//typedef std::variant<num_t, std::string, err_t, rng_t> value_t;
typedef std::vector<value_t> values;

typedef std::vector<std::string> strings;

class cell;
typedef cell cell_t;
typedef cell CELL;
typedef uint32_t coord_t; // definition cell location

//typedef std::unordered_set<rng_t*> ranges_t;
typedef std::vector<rng_t> ranges_t;
