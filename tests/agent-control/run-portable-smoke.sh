#!/bin/sh
set -eu
# Run from the chosen published source checkout. No game assets are used.
CXX="${CXX:-c++}"
OUT="${OUT:-$(mktemp -d)}"
mkdir -p "$OUT"
for name in AgentGlobeProjectionTest AgentSocketContractTest AgentTacticalGridTest; do
  "$CXX" -std=c++11 -O2 -Wall -Wextra -pedantic -DOPENXCOM_EXPERIMENTAL_AGENT_CONTROL     "tests/agent-control/$name.cpp" -o "$OUT/$name"
  "$OUT/$name"
done
"$CXX" -std=c++11 -O2 -Wall -Wextra -pedantic   tests/agent-control/AgentProjectileWitnessTest.cpp src/Observer/AgentProjectileWitness.cpp   -o "$OUT/AgentProjectileWitnessTest"
"$OUT/AgentProjectileWitnessTest"
for name in AgentPublicEventsTest; do
  "$CXX" -std=c++11 -O2 -Wall -Wextra -pedantic     "tests/agent-control/$name.cpp" src/Observer/AgentProtocol.cpp     src/Observer/AgentProjectileWitness.cpp -o "$OUT/$name"
  "$OUT/$name"
done
