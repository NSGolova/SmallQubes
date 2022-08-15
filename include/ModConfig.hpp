#pragma once
#include "config-utils/shared/config-utils.hpp"

DECLARE_CONFIG(ModConfig,

    CONFIG_VALUE(QubeSize, float, "Cube size", 0.85, "Yes");

    CONFIG_INIT_FUNCTION(
        CONFIG_INIT_VALUE(QubeSize);
    )
)