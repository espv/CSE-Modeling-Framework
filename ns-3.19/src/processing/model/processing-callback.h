#ifndef PROCESSING_CALLBACK_H
#define PROCESSING_CALLBACK_H

#include <vector>

#include "ns3/object.h"
#include "ns3/empty.h"
#include "ns3/event-impl.h"
#include "ns3/type-traits.h"

namespace ns3 {

class ProcessingCallbackBase : public SimpleRefCount<ProcessingCallbackBase>
{
 public:
  ProcessingCallbackBase() {}
  virtual ~ProcessingCallbackBase() {}
  virtual void run() {}

  virtual void SetArgs(Ptr<ProcessingCallbackBase> args) {}
};

template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
class ProcessingCallbackArgs : public ProcessingCallbackBase
{
public:
  ProcessingCallbackArgs() {};

  virtual void run() {}

  T1 arg1;
  T2 arg2;
  T3 arg3;
  T4 arg4;
  T5 arg5;
  T6 arg6;
  T7 arg7;
  T8 arg8;

  void SetArgs(T1 arg1, T2 arg2, T3 arg3, T4 arg4, T5 arg5, T6 arg6, T7 arg7, T8 arg8) {
    this->arg1 = arg1;
    this->arg2 = arg2;
    this->arg3 = arg3;
    this->arg4 = arg4;
    this->arg5 = arg5;
    this->arg6 = arg6;
    this->arg7 = arg7;
    this->arg8 = arg8;
  }

  virtual void SetArgs(Ptr<ProcessingCallbackBase> args) {
    arg1 = DynamicCast<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, T8> > (args)->arg1;
    arg2 = DynamicCast<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, T8> > (args)->arg2;
    arg3 = DynamicCast<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, T8> > (args)->arg3;
    arg4 = DynamicCast<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, T8> > (args)->arg4;
    arg5 = DynamicCast<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, T8> > (args)->arg5;
    arg6 = DynamicCast<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, T8> > (args)->arg6;
    arg7 = DynamicCast<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, T8> > (args)->arg7;
    arg8 = DynamicCast<ProcessingCallbackArgs<T1, T2, T3, T4, T5, T6, T7, T8> > (args)->arg8;
  }

  virtual ~ProcessingCallbackArgs() {}
};

template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class MEM, class OBJ>
class ProcessingCallbackFunction: public ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,T8>
{
public:
  ProcessingCallbackFunction() {}
  virtual ~ProcessingCallbackFunction() {}

  MEM m_function; OBJ m_object;

  virtual void run() { (EventMemberImplObjTraits<OBJ>::GetReference (m_object).*m_function)(ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,T8>::arg1,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,T8>::arg2,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,T8>::arg3,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,T8>::arg4,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,T8>::arg5,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,T8>::arg6,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,T8>::arg7,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,T8>::arg8);
  }
};

template<class MEM, class OBJ>
class ProcessingCallbackFunction<empty,empty,empty,empty,empty,empty,empty,empty,MEM,OBJ>: public ProcessingCallbackArgs<empty,empty,empty,empty,empty,empty,empty,empty>
{
public:
  ProcessingCallbackFunction() {}
  virtual ~ProcessingCallbackFunction() {}

  MEM m_function; OBJ m_object;
  virtual void run() { (EventMemberImplObjTraits<OBJ>::GetReference (m_object).*m_function)(); 
  }
};

template<class T1, class MEM, class OBJ>
class ProcessingCallbackFunction<T1,empty,empty,empty,empty,empty,empty,empty,MEM,OBJ>: public ProcessingCallbackArgs<T1,empty,empty,empty,empty,empty,empty,empty>
{
public:
  ProcessingCallbackFunction() {}
  virtual ~ProcessingCallbackFunction() {}

  MEM m_function; OBJ m_object;
  virtual void run() { (EventMemberImplObjTraits<OBJ>::GetReference (m_object).*m_function)(ProcessingCallbackArgs<T1,empty,empty,empty,empty,empty,empty,empty>::arg1);
  }
};

template<class T1,class T2, class MEM, class OBJ>
class ProcessingCallbackFunction<T1,T2,empty,empty,empty,empty,empty,empty,MEM,OBJ>: public ProcessingCallbackArgs<T1,T2,empty,empty,empty,empty,empty,empty>
{
public:
  ProcessingCallbackFunction() {}
  virtual ~ProcessingCallbackFunction() {}

  MEM m_function; OBJ m_object;
  virtual void run() { (EventMemberImplObjTraits<OBJ>::GetReference (m_object).*m_function)(ProcessingCallbackArgs<T1,T2,empty,empty,empty,empty,empty,empty>::arg1,
											 ProcessingCallbackArgs<T1,T2,empty,empty,empty,empty,empty,empty>::arg2);
  }
};

template<class T1,class T2, class T3, class MEM, class OBJ>
class ProcessingCallbackFunction<T1,T2,T3,empty,empty,empty,empty,empty,MEM,OBJ>: public ProcessingCallbackArgs<T1,T2,T3,empty,empty,empty,empty,empty>
{
public:
  ProcessingCallbackFunction() {}
  virtual ~ProcessingCallbackFunction() {}

  MEM m_function; OBJ m_object;
  virtual void run() { (EventMemberImplObjTraits<OBJ>::GetReference (m_object).*m_function)(ProcessingCallbackArgs<T1,T2,T3,empty,empty,empty,empty,empty>::arg1,
											 ProcessingCallbackArgs<T1,T2,T3,empty,empty,empty,empty,empty>::arg2,
											 ProcessingCallbackArgs<T1,T2,T3,empty,empty,empty,empty,empty>::arg3);
  }
};

template<class T1,class T2, class T3, class T4, class MEM, class OBJ>
class ProcessingCallbackFunction<T1,T2,T3,T4,empty,empty,empty,empty,MEM,OBJ>: public ProcessingCallbackArgs<T1,T2,T3,T4,empty,empty,empty,empty>
{
public:
  ProcessingCallbackFunction() {}
  virtual ~ProcessingCallbackFunction() {}

  MEM m_function; OBJ m_object;
  virtual void run() { (EventMemberImplObjTraits<OBJ>::GetReference (m_object).*m_function)(ProcessingCallbackArgs<T1,T2,T3,T4,empty,empty,empty,empty>::arg1,
											 ProcessingCallbackArgs<T1,T2,T3,T4,empty,empty,empty,empty>::arg2,
											 ProcessingCallbackArgs<T1,T2,T3,T4,empty,empty,empty,empty>::arg3,
											 ProcessingCallbackArgs<T1,T2,T3,T4,empty,empty,empty,empty>::arg4);
  }
};

template<class T1,class T2, class T3, class T4, class T5, class MEM, class OBJ>
class ProcessingCallbackFunction<T1,T2,T3,T4,T5,empty,empty,empty,MEM,OBJ>: public ProcessingCallbackArgs<T1,T2,T3,T4,T5,empty,empty,empty>
{
public:
  ProcessingCallbackFunction() {}
  virtual ~ProcessingCallbackFunction() {}

  MEM m_function; OBJ m_object;
  virtual void run() { (EventMemberImplObjTraits<OBJ>::GetReference (m_object).*m_function)(ProcessingCallbackArgs<T1,T2,T3,T4,T5,empty,empty,empty>::arg1,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,empty,empty,empty>::arg2,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,empty,empty,empty>::arg3,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,empty,empty,empty>::arg4,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,empty,empty,empty>::arg5);
  }
};

template<class T1,class T2, class T3, class T4, class T5, class T6, class MEM, class OBJ>
class ProcessingCallbackFunction<T1,T2,T3,T4,T5,T6,empty,empty,MEM,OBJ>: public ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,empty,empty>
{
public:
  ProcessingCallbackFunction() {}
  virtual ~ProcessingCallbackFunction() {}

  MEM m_function; OBJ m_object;
  virtual void run() { (EventMemberImplObjTraits<OBJ>::GetReference (m_object).*m_function)(ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,empty,empty>::arg1,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,empty,empty>::arg2,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,empty,empty>::arg3,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,empty,empty>::arg4,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,empty,empty>::arg5,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,empty,empty>::arg6);
  }
};

template<class T1,class T2, class T3, class T4, class T5, class T6, class T7, class MEM, class OBJ>
class ProcessingCallbackFunction<T1,T2,T3,T4,T5,T6,T7,empty,MEM,OBJ>: public ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,empty>
{
public:
  ProcessingCallbackFunction() {}
  virtual ~ProcessingCallbackFunction() {}

  MEM m_function; OBJ m_object;
  virtual void run() { (EventMemberImplObjTraits<OBJ>::GetReference (m_object).*m_function)(ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,empty>::arg1,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,empty>::arg2,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,empty>::arg3,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,empty>::arg4,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,empty>::arg5,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,empty>::arg6,
											 ProcessingCallbackArgs<T1,T2,T3,T4,T5,T6,T7,empty>::arg7);
  }
};

} // namespace ns3

#endif // PROCESSING_CALLBACK_H
