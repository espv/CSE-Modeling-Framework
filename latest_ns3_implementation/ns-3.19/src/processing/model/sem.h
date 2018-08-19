#ifndef SEM_H
#define SEM_H

#include "peu.h"

namespace ns3 {

class PEU;
class Program;
class LoopCondition;

class SEM : public SimpleRefCount<SEM>
{
 public:
  SEM();

  // We need to state which PEU on which this SEM 
  // belongs. This information is used by the
  // HWModel.
  Ptr<PEU> peu;

  // For now, we simply chose the closest one, i.e.,
  // we add interpolation after we get the whole model
  // up and running.
  //
  // DEPRECATED: Conditions are used to get programs
  //  Program* GetProgram(Ptr<Packet> p);
  // In stead, we have one single root program, which
  // may contain conditions or not.
  Program *rootProgram;

  // Since loops are now modelled as services, we must have
  // a pointer to a loop condition here that is consulted
  // to determine whether or not to re-iterate.
  // Set to NULL if service is not loop.
  LoopCondition *lc;

  // Service triggers are stored in the respective SEMs
  std::string trigger;

  // DEBUG
  std::string name;

  // For execution statistics
  unsigned long long numExec;
  unsigned long long numStmtExec;
  unsigned long long cpuProcessing;
  unsigned long long peuProcessing;
  unsigned long long blocked;

 private:
};

}

#endif
