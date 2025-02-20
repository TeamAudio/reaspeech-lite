#ifndef _REAPER_VST3_INTERFACES_H_
#define _REAPER_VST3_INTERFACES_H_

class IReaperHostApplication : public FUnknown // available from IHostApplication in REAPER v5.02+
{
public:
  // Gets a REAPER Extension API function by name, returns NULL is failed
  virtual void* PLUGIN_API getReaperApi(CStringA funcname) = 0;

  virtual void* PLUGIN_API getReaperParent(uint32 w) = 0; // get parent track(=1), take(=2), project(=3), fxdsp(=4), trackchan(=5)

  // Multi-purpose function, returns NULL if unsupported
  virtual void* PLUGIN_API reaperExtended(uint32 call, void *parm1, void *parm2, void *parm3) = 0;

  static const FUID iid;
};

DECLARE_CLASS_IID (IReaperHostApplication, 0x79655E36, 0x77EE4267, 0xA573FEF7, 0x4912C27C)
#endif
