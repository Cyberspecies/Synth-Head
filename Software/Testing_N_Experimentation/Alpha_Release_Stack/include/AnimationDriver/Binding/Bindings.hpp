/*****************************************************************
 * @file Bindings.hpp
 * @brief Master include for all binding components
 *****************************************************************/

#pragma once

#include "FilterBase.hpp"
#include "SpringFilter.hpp"
#include "FilterChain.hpp"
#include "ValueBinding.hpp"
#include "SensorBinding.hpp"

// Note: IMUBinding.hpp is deprecated. Use SensorBinding with SensorHub instead.
// The new system supports any sensor type (IMU, GPS, humidity, temperature, etc.)
