// SPDX-License-Identifier: GPL-3.0-or-later
// Literal status parser slice/reply bodies. Ledger, send and close are explicit seams;
// no full poll admission, socket scheduling, game or UI execution is represented.
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <iostream>
#include "Token.generated.inc"
struct StanceController {
 struct Entry {std::string request,status; bool operator==(const Entry&o)const{return request==o.request&&status==o.status;}};
 std::map<std::string,Entry> _commands;
 std::string _epoch=std::string(32,'e');
 bool _selectionEnabled=false,_turnEnabled=false,_mapLeftClickEnabled=false;
 bool closed=false; unsigned downstream=0;
 std::vector<std::string> sent;
 void sendPacket(const std::string&s){sent.push_back(s);}
 void close(){closed=true;}
 void unavailable();
 void replyStatus(const std::string&,const std::string& = std::string());
 void hello(){
#include "Hello.generated.inc"
 }
 void parse(const std::string&request){const auto n=request.size();
#include "Parser.generated.inc"
 ++downstream;
 }
};
#include "Replies.generated.inc"
static unsigned checks=0;
static void check(bool condition,const char*description){++checks;if(!condition){std::cerr<<"FAIL "<<description<<'\n';std::exit(1);}}
static std::string packet(const std::string&id,const std::string&status,const std::string&nonce){return "{\"schema\":\"experimental-stance-status/2\",\"command_id\":\""+id+"\",\"status\":\""+status+"\",\"query_nonce\":"+(nonce.empty()?"null":"\""+nonce+"\"")+"}";}
int main(){
 const std::string id(32,'a'), other(32,'b'), nonce=std::string(31,'0')+"1";
 StanceController hello;hello.hello();check(hello.sent==std::vector<std::string>{"{\"schema\":\"experimental-stance/2\",\"control_epoch\":\""+hello._epoch+"\"}"},"actual hello send expression version2");
 for(const std::string status:{"unavailable","pending_confirmation","dispatched"}){
  StanceController c;c._commands[id]={"original request",status};const auto ledger=c._commands;
  c.replyStatus(id);check(c.sent==std::vector<std::string>{packet(id,status,"")},"initial status has null query nonce");
  c.sent.clear();c.parse("S"+id+nonce);check(c.sent==std::vector<std::string>{packet(id,status,nonce)},"known status query echoes exact nonce id status");
  check(c._commands==ledger&&!c.closed&&c.downstream==0,"query preserves ledger no action path");
  c.sent.clear();const std::string next(32,'f');c.parse("S"+id+next);check(c.sent==std::vector<std::string>{packet(id,status,next)},"subsequent nonce exact not stale echo");
  check(c._commands==ledger&&c.downstream==0,"repeated queries do not replay action");
 }
 std::vector<std::string> invalid={"","S","S"+id,"S"+id+std::string(32,'0'),"S"+id+std::string(32,'G'),"S"+std::string(32,'A')+nonce,"S"+id+nonce+"x","S"+id+nonce.substr(1),"T"+id+nonce,"S"+id+std::string(31,'1')+"\n"};
 for(const auto &wire:invalid){StanceController c;c._commands[id]={"original","pending_confirmation"};auto ledger=c._commands;c.parse(wire);check(c.sent==std::vector<std::string>{"{\"error\":\"unavailable\"}"}&&c.closed,"actual native fallback refuses malformed or old status wire");check(c._commands==ledger&&c.downstream==0,"rejection no ledger or dispatch mutation");}
 StanceController missing;missing._commands[id]={"original","dispatched"};auto ledger=missing._commands;missing.parse("S"+other+nonce);check(missing.sent==std::vector<std::string>{"{\"error\":\"unavailable\"}"}&&!missing.closed,"unknown command native unavailable semantics unchanged");check(missing._commands==ledger&&missing.downstream==0,"unknown command not inserted or dispatched");
 for(const auto &bad:std::vector<std::string>{std::string(32,'0'),std::string(32,'Z'),"short"}){StanceController c;c._commands[id]={"original","dispatched"};auto before=c._commands;c.replyStatus(id,bad);check(c.closed&&c.sent==std::vector<std::string>{"{\"error\":\"unavailable\"}"}&&c._commands==before,"reply helper independently refuses invalid nonce without ledger mutation");}
 std::cout<<"PASS "<<checks<<" literal supervised status-query checks; explicit transport/ledger seams\n";
}
