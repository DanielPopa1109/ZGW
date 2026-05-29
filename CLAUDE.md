# AUTOSAR DIY Lab Project

## Project Context
- Hand-written AUTOSAR Classic Platform implementation
- Lab/educational project — functional correctness is the priority
- All modules open for modification, nothing is off-limits
- No RTE — SWCs communicate directly, this is intentional and acceptable
- No vendor tooling or generated code, everything is handwritten

## Analysis Approach
- Analyze and review code statically — no builds, no test execution
- Focus on logical correctness and interface contracts between BSW modules
- MISRA-C compliance is not required
- Deviations from AUTOSAR standard are acceptable if functionality works
- Suggest fixes at source code level only