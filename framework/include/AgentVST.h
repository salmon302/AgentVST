/**
 * AgentVST.h — Master include
 *
 * Agents only need to include AgentDSP.h.
 * Framework consumers can include this header to get everything.
 */
#pragma once

#include "AgentDSP.h"
#include "SchemaParser.h"
#include "ParameterCache.h"
#include "DSPNode.h"
#include "ProcessBlockHandler.h"
#include "UIGenerator.h"

// Pre-built DSP modules
#include "../src/modules/BiquadFilter.h"
#include "../src/modules/Gain.h"
#include "../src/modules/EnvelopeFollower.h"
#include "../src/modules/Oscillator.h"
