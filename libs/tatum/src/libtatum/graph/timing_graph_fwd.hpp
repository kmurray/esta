#pragma once
/*
 * Forward declarations for Timing Graph and related types
 */

//The timing graph
class TimingGraph;

//Potential node types in the timing graph
enum class TN_Type;
enum class TE_Type;

//Various IDs used by the timing graph
typedef unsigned short NodeId;
typedef int BlockId;
typedef int EdgeId;
typedef char DomainId;
typedef int LevelId;

#define INVALID_CLOCK_DOMAIN -1

