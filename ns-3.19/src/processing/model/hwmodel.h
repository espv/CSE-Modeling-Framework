/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2004 Francisco J. Ros
 * Copyright (c) 2007 INESC Porto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Francisco J. Ros  <fjrm@dif.um.es>
 *          Gustavo J. A. M. Carneiro <gjc@inescporto.pt>
 */

#ifndef HWMODEL_H
#define HWMODEL_H

#include "ns3/object.h"
#include "ns3/event-garbage-collector.h"
#include "ns3/timer.h"
#include "ns3/traced-callback.h"
#include "ns3/random-variable.h"
#include "ns3/taskscheduler.h"
#include "ns3/node.h"

#include <vector>
#include <stack>
#include <map>
#include <string>

namespace ns3 {

class CPU;
class InterruptController;
class MemBus;

class HWModel : public Object
{
public:
  static TypeId GetTypeId (void);

  HWModel ();
  virtual ~HWModel ();

  // We need a pointer to the Processing
  // object.
  Ptr<Node> node;

  // The CPU is a special PEU
  // Ptr<CPU> cpu;
  // OYSTEDAL: The hardware contains a set of cpus
  std::vector<Ptr<CPU> > cpus;
  Ptr<CPU> AddCPU(Ptr<CPU>);

  // Components of the hardware model:
  // - Physical execution units
  // - Memory bus

  // - Interrupt controller
  std::map<std::string, Ptr<PEU> > m_PEUs;
  Ptr<MemBus> m_memBus;
#if 0
  Ptr<APIC> m_interruptController;
#else
  Ptr<InterruptController> m_interruptController;
#endif

  // Needed to calculate the number of
  // memory lookups required for every
  // cache miss.
  int cacheLineSize;

};

} // namespace ns3

#endif /* HWMODEL_H */
