// SPDX-License-Identifier: GPL-3.0-or-later
// Actual bootstrap loop + native models; no Game, resources, saves or window.
#include <memory>
#include <stdexcept>
#include <iostream>
#include "../../src/Mod/Mod.h"
#include "../../src/Mod/RuleSoldier.h"
#include "../../src/Mod/Armor.h"
#include "../../src/Savegame/SavedGame.h"
#include "../../src/Savegame/Base.h"
#include "../../src/Savegame/Soldier.h"
#include "../../src/Savegame/SoldierDiary.h"
#include "../../src/Savegame/SoldierDeath.h"
#include "../../src/Savegame/MissionStatistics.h"
using namespace OpenXcom;
void require(bool ok){if(!ok)throw std::runtime_error("memorial fixture invariant");}
int main(){
 Mod actualMod;Mod *mod=&actualMod;
 RuleSoldier actualRules("TEST_SOLDIER");RuleSoldier *rules=&actualRules;
 Armor actualArmor("TEST_ARMOR");Armor *armor=&actualArmor;
 std::unique_ptr<SavedGame> campaign(new SavedGame());
 Base *base=new Base(mod);campaign->getBases()->push_back(base);
 Soldier *original=new Soldier(rules,armor,0);original->setName("Retained living");base->getSoldiers()->push_back(original);
 const size_t living=1;
 #include "MemorialSetup.generated.inc"
 require(base->getSoldiers()->size()==1 && base->getSoldiers()->front()==original);
 require(campaign->getDeadSoldiers()->size()==3 && campaign->getMissionStatistics()->size()==3);
 for(size_t i=0;i<3;++i){
  Soldier *s=campaign->getDeadSoldiers()->at(i);
  require(s->getDeath() && s->getMissions()==1 && s->getRank()==i && s->getId()>0);
  require(s->getDiary()->getMissionIdList().size()==1 && s->getDiary()->getMissionIdList()[0]==int(i));
  const YAML::Node saved=s->save();
  require(saved["gender"].as<int>()>=0 && saved["look"].as<int>()>=0 && saved["name"].as<std::string>()==s->getName());
 }
 std::cout<<"PASS actual memorial setup loop: native constructor, 3 deaths/history joins, living roster preserved\n";
}
