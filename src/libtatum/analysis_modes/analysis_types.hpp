#include "SetupAnalysisMode.hpp"
#include "HoldAnalysisMode.hpp"

/** \file
 * Useful shorthands for the supported analysis types
 *
 * Used as template parameters to specify timing analysis
 * mode for TimingAnalyzer and derived classes
 */

///Setup (max) analysis
using SetupAnalysis = SetupAnalysisMode<>;

///Hold (min) analysis
using HoldAnalysis = HoldAnalysisMode<>;

///Combined Setup *and* Hold analysis
using SetupHoldAnalysis = HoldAnalysisMode<SetupAnalysisMode<>>;
