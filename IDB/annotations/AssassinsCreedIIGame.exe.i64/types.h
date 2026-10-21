// Local types of AssassinsCreedIIGame.exe.i64, one block per type, sorted by name. Written by
// tools/ida/annotations.py; edit types in IDA, not here.

// @type ADDRESS_FAMILY
typedef USHORT ADDRESS_FAMILY;

// @type AK::MemoryMgr
struct AK::MemoryMgr;

// @type AK::Monitor
struct AK::Monitor;

// @type AK::MusicEngine
struct AK::MusicEngine;

// @type AK::SoundEngine
struct AK::SoundEngine;

// @type AK::SoundEngine::DynamicDialogue
struct AK::SoundEngine::DynamicDialogue;

// @type AK::SoundEngine::DynamicSequence
struct AK::SoundEngine::DynamicSequence;

// @type AK::StreamMgr
struct AK::StreamMgr;

// @type BOOL
typedef int BOOL;

// @type BYTE
typedef unsigned __int8 BYTE;

// @type CHAR
typedef char CHAR;

// @type CONTEXT
typedef struct _CONTEXT CONTEXT;

// @type COR_ILMETHOD_SECT
struct COR_ILMETHOD_SECT;

// @type CPPEH_RECORD
struct CPPEH_RECORD
{
  DWORD old_esp;
  EXCEPTION_POINTERS *exc_ptr;
  struct _EH3_EXCEPTION_REGISTRATION registration;
};

// @type CatchableType
typedef const struct _s_CatchableType CatchableType;

// @type CatchableTypeArray
typedef const struct _s_CatchableTypeArray CatchableTypeArray;

// @type Concurrency::details::ContextBase
struct Concurrency::details::ContextBase;

// @type Concurrency::details::ExecutionResource
struct Concurrency::details::ExecutionResource;

// @type Concurrency::details::ExternalContextBase
struct Concurrency::details::ExternalContextBase;

// @type Concurrency::details::ExternalStatistics
struct Concurrency::details::ExternalStatistics;

// @type Concurrency::details::HardwareAffinity
struct Concurrency::details::HardwareAffinity;

// @type Concurrency::details::InternalContextBase
struct Concurrency::details::InternalContextBase;

// @type Concurrency::details::LockQueueNode
struct Concurrency::details::LockQueueNode;

// @type Concurrency::details::ResourceManager
struct Concurrency::details::ResourceManager;

// @type Concurrency::details::ScheduleGroupSegmentBase
struct Concurrency::details::ScheduleGroupSegmentBase;

// @type Concurrency::details::SchedulerBase
struct Concurrency::details::SchedulerBase;

// @type Concurrency::details::SchedulerProxy
struct Concurrency::details::SchedulerProxy;

// @type Concurrency::details::SchedulingRing
struct Concurrency::details::SchedulingRing;

// @type Concurrency::details::TaskStack
struct Concurrency::details::TaskStack;

// @type Concurrency::details::UMSFreeThreadProxy
struct Concurrency::details::UMSFreeThreadProxy;

// @type Concurrency::details::UMSSchedulerProxy
struct Concurrency::details::UMSSchedulerProxy;

// @type Concurrency::details::UMSThreadScheduler
struct Concurrency::details::UMSThreadScheduler;

// @type Concurrency::details::UMSThreadVirtualProcessor
struct Concurrency::details::UMSThreadVirtualProcessor;

// @type Concurrency::details::VirtualProcessor
struct Concurrency::details::VirtualProcessor;

// @type Concurrency::details::VirtualProcessorRoot
struct Concurrency::details::VirtualProcessorRoot;

// @type Concurrency::details::WorkItem
struct Concurrency::details::WorkItem;

// @type Concurrency::details::WorkQueue
struct Concurrency::details::WorkQueue;

// @type Concurrency::details::_NonReentrantLock
struct Concurrency::details::_NonReentrantLock;

// @type Concurrency::details::_ReaderWriterLock
struct Concurrency::details::_ReaderWriterLock;

// @type Concurrency::details::_UnrealizedChore
struct Concurrency::details::_UnrealizedChore;

// @type DName
struct DName;

// @type DNameNode
struct DNameNode;

// @type DNameStatusNode
struct DNameStatusNode;

// @type DWORD
typedef unsigned int DWORD;

// @type DWORD64
typedef unsigned __int64 DWORD64;

// @type DWORD_PTR
typedef ULONG_PTR DWORD_PTR;

// @type EH3_EXCEPTION_REGISTRATION
typedef struct _EH3_EXCEPTION_REGISTRATION EH3_EXCEPTION_REGISTRATION;

// @type EXCEPTION_DISPOSITION
typedef enum _EXCEPTION_DISPOSITION EXCEPTION_DISPOSITION;

// @type EXCEPTION_RECORD
typedef struct _EXCEPTION_RECORD EXCEPTION_RECORD;

// @type EXCEPTION_ROUTINE
typedef EXCEPTION_DISPOSITION __stdcall EXCEPTION_ROUTINE(struct _EXCEPTION_RECORD *ExceptionRecord, PVOID EstablisherFrame, struct _CONTEXT *ContextRecord, PVOID DispatcherContext);

// @type FILE
typedef struct _iobuf FILE;

// @type FILETIME
typedef struct _FILETIME FILETIME;

// @type FLOATING_SAVE_AREA
typedef struct _FLOATING_SAVE_AREA FLOATING_SAVE_AREA;

// @type FuncInfo
struct FuncInfo
{
  int magicNumber __hex;
  int maxState __udec;
  _UNKNOWN *pUnwindMap;
  int nTryBlocks __udec;
  _UNKNOWN *pTryBlockMap;
  int nIPMapEntries __udec;
  _UNKNOWN *pIPtoStateMap;
  _UNKNOWN *pESTypeList;
  int EHFlags __hex;
};

// @type GUID
typedef struct _GUID GUID;

// @type HANDLE
typedef void *HANDLE;

// @type HARDWAREINPUT
typedef struct tagHARDWAREINPUT HARDWAREINPUT;

// @type HBRUSH
typedef struct HBRUSH__ *HBRUSH;

// @type HBRUSH__
struct HBRUSH__
{
  int unused;
};

// @type HCURSOR
typedef HICON HCURSOR;

// @type HICON
typedef struct HICON__ *HICON;

// @type HICON__
struct HICON__
{
  int unused;
};

// @type HINSTANCE
typedef struct HINSTANCE__ *HINSTANCE;

// @type HINSTANCE__
struct HINSTANCE__
{
  int unused;
};

// @type HKEY
typedef struct HKEY__ *HKEY;

// @type HKEY__
struct HKEY__
{
  int unused;
};

// @type HMODULE
typedef HINSTANCE HMODULE;

// @type HMONITOR
typedef struct HMONITOR__ *HMONITOR;

// @type HMONITOR__
struct HMONITOR__
{
  int unused;
};

// @type HRESULT
typedef int HRESULT;

// @type HWND
typedef struct HWND__ *HWND;

// @type HWND__
struct HWND__
{
  int unused;
};

// @type HandlerType
struct HandlerType
{
  int adjectives __hex;
  _UNKNOWN *pType;
  int dispCatchObj __dec __tabform(NODUPS,1);
  _UNKNOWN *addressOfHandler;
};

// @type HeapManager
struct HeapManager;

// @type IFTYPE
typedef ULONG IFTYPE;

// @type IF_INDEX
typedef NET_IFINDEX IF_INDEX;

// @type IID
typedef GUID IID;

// @type INTERNAL_IF_OPER_STATUS
enum INTERNAL_IF_OPER_STATUS : __int32
{
  IF_OPER_STATUS_NON_OPERATIONAL = 0x0,
  IF_OPER_STATUS_UNREACHABLE = 0x1,
  IF_OPER_STATUS_DISCONNECTED = 0x2,
  IF_OPER_STATUS_CONNECTING = 0x3,
  IF_OPER_STATUS_CONNECTED = 0x4,
  IF_OPER_STATUS_OPERATIONAL = 0x5,
};

// @type IPAddr
typedef ULONG IPAddr;

// @type IUnknown
#pragma pack(push, 8)
struct IUnknown
{
  struct IUnknownVtbl *lpVtbl;
};
#pragma pack(pop)

// @type IUnknownVtbl
#pragma pack(push, 8)
struct IUnknownVtbl
{
  HRESULT (__stdcall *QueryInterface)(IUnknown *This, const IID *const riid, void **ppvObject);
  ULONG (__stdcall *AddRef)(IUnknown *This);
  ULONG (__stdcall *Release)(IUnknown *This);
};
#pragma pack(pop)

// @type KEYBDINPUT
typedef struct tagKEYBDINPUT KEYBDINPUT;

// @type LARGE_INTEGER
typedef union _LARGE_INTEGER LARGE_INTEGER;

// @type LCID
typedef DWORD LCID;

// @type LCTYPE
typedef DWORD LCTYPE;

// @type LIST_ENTRY
typedef struct _LIST_ENTRY LIST_ENTRY;

// @type LONG
typedef int LONG;

// @type LONGLONG
typedef __int64 LONGLONG;

// @type LONG_PTR
typedef int LONG_PTR;

// @type LPARAM
typedef LONG_PTR LPARAM;

// @type LPBYTE
typedef BYTE *LPBYTE;

// @type LPCCH
typedef const CHAR *LPCCH;

// @type LPCRITICAL_SECTION
typedef PRTL_CRITICAL_SECTION LPCRITICAL_SECTION;

// @type LPCSTR
typedef const CHAR *LPCSTR;

// @type LPCVOID
typedef const void *LPCVOID;

// @type LPCWCH
typedef const WCHAR *LPCWCH;

// @type LPCWSTR
typedef const WCHAR *LPCWSTR;

// @type LPDWORD
typedef DWORD *LPDWORD;

// @type LPOVERLAPPED
typedef struct _OVERLAPPED *LPOVERLAPPED;

// @type LPSECURITY_ATTRIBUTES
typedef struct _SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;

// @type LPSTR
typedef CHAR *LPSTR;

// @type LPSYSTEMTIME
typedef struct _SYSTEMTIME *LPSYSTEMTIME;

// @type LPTHREAD_START_ROUTINE
typedef PTHREAD_START_ROUTINE LPTHREAD_START_ROUTINE;

// @type LPUNKNOWN
typedef IUnknown *LPUNKNOWN;

// @type LPVOID
typedef void *LPVOID;

// @type LPWORD
typedef WORD *LPWORD;

// @type LPWSADATA
typedef struct WSAData *LPWSADATA;

// @type LPWSTR
typedef WCHAR *LPWSTR;

// @type LRESULT
typedef LONG_PTR LRESULT;

// @type MINIDUMP_CALLBACK_ROUTINE
typedef BOOL (__stdcall *MINIDUMP_CALLBACK_ROUTINE)(PVOID CallbackParam, PMINIDUMP_CALLBACK_INPUT CallbackInput, PMINIDUMP_CALLBACK_OUTPUT CallbackOutput);

// @type MINIDUMP_INCLUDE_MODULE_CALLBACK
typedef struct _MINIDUMP_INCLUDE_MODULE_CALLBACK MINIDUMP_INCLUDE_MODULE_CALLBACK;

// @type MINIDUMP_INCLUDE_THREAD_CALLBACK
typedef struct _MINIDUMP_INCLUDE_THREAD_CALLBACK MINIDUMP_INCLUDE_THREAD_CALLBACK;

// @type MINIDUMP_IO_CALLBACK
typedef struct _MINIDUMP_IO_CALLBACK MINIDUMP_IO_CALLBACK;

// @type MINIDUMP_MEMORY_INFO
typedef struct _MINIDUMP_MEMORY_INFO MINIDUMP_MEMORY_INFO;

// @type MINIDUMP_MODULE_CALLBACK
typedef struct _MINIDUMP_MODULE_CALLBACK MINIDUMP_MODULE_CALLBACK;

// @type MINIDUMP_READ_MEMORY_FAILURE_CALLBACK
typedef struct _MINIDUMP_READ_MEMORY_FAILURE_CALLBACK MINIDUMP_READ_MEMORY_FAILURE_CALLBACK;

// @type MINIDUMP_THREAD_CALLBACK
typedef struct _MINIDUMP_THREAD_CALLBACK MINIDUMP_THREAD_CALLBACK;

// @type MINIDUMP_THREAD_EX_CALLBACK
typedef struct _MINIDUMP_THREAD_EX_CALLBACK MINIDUMP_THREAD_EX_CALLBACK;

// @type MINIDUMP_TYPE
enum MINIDUMP_TYPE : __int32
{
  MiniDumpNormal = 0x0,
  MiniDumpWithDataSegs = 0x1,
  MiniDumpWithFullMemory = 0x2,
  MiniDumpWithHandleData = 0x4,
  MiniDumpFilterMemory = 0x8,
  MiniDumpScanMemory = 0x10,
  MiniDumpWithUnloadedModules = 0x20,
  MiniDumpWithIndirectlyReferencedMemory = 0x40,
  MiniDumpFilterModulePaths = 0x80,
  MiniDumpWithProcessThreadData = 0x100,
  MiniDumpWithPrivateReadWriteMemory = 0x200,
  MiniDumpWithoutOptionalData = 0x400,
  MiniDumpWithFullMemoryInfo = 0x800,
  MiniDumpWithThreadInfo = 0x1000,
  MiniDumpWithCodeSegs = 0x2000,
  MiniDumpWithoutAuxiliaryState = 0x4000,
  MiniDumpWithFullAuxiliaryState = 0x8000,
  MiniDumpWithPrivateWriteCopyMemory = 0x10000,
  MiniDumpIgnoreInaccessibleMemory = 0x20000,
  MiniDumpWithTokenInformation = 0x40000,
  MiniDumpWithModuleHeaders = 0x80000,
  MiniDumpFilterTriage = 0x100000,
  MiniDumpWithAvxXStateContext = 0x200000,
  MiniDumpWithIptTrace = 0x400000,
  MiniDumpValidTypeFlags = 0x7FFFFF,
};

// @type MINIDUMP_VM_POST_READ_CALLBACK
typedef struct _MINIDUMP_VM_POST_READ_CALLBACK MINIDUMP_VM_POST_READ_CALLBACK;

// @type MINIDUMP_VM_PRE_READ_CALLBACK
typedef struct _MINIDUMP_VM_PRE_READ_CALLBACK MINIDUMP_VM_PRE_READ_CALLBACK;

// @type MINIDUMP_VM_QUERY_CALLBACK
typedef struct _MINIDUMP_VM_QUERY_CALLBACK MINIDUMP_VM_QUERY_CALLBACK;

// @type MOUSEINPUT
typedef struct tagMOUSEINPUT MOUSEINPUT;

// @type NET_IFINDEX
typedef ULONG NET_IFINDEX;

// @type PCONTEXT
typedef CONTEXT *PCONTEXT;

// @type PDWORD
typedef DWORD *PDWORD;

// @type PEH3_EXCEPTION_REGISTRATION
typedef struct _EH3_EXCEPTION_REGISTRATION *PEH3_EXCEPTION_REGISTRATION;

// @type PEXCEPTION_POINTERS
typedef struct _EXCEPTION_POINTERS *PEXCEPTION_POINTERS;

// @type PEXCEPTION_RECORD
typedef EXCEPTION_RECORD *PEXCEPTION_RECORD;

// @type PEXCEPTION_ROUTINE
typedef EXCEPTION_ROUTINE *PEXCEPTION_ROUTINE;

// @type PLARGE_INTEGER
typedef LARGE_INTEGER *PLARGE_INTEGER;

// @type PMD
#pragma pack(push, 4)
struct PMD
{
  int mdisp;
  int pdisp;
  int vdisp;
};
#pragma pack(pop)

// @type PMFN
typedef void (__cdecl *PMFN)(void *);

// @type PMIB_IFROW
typedef struct _MIB_IFROW *PMIB_IFROW;

// @type PMINIDUMP_CALLBACK_INFORMATION
typedef struct _MINIDUMP_CALLBACK_INFORMATION *PMINIDUMP_CALLBACK_INFORMATION;

// @type PMINIDUMP_CALLBACK_INPUT
typedef struct _MINIDUMP_CALLBACK_INPUT *PMINIDUMP_CALLBACK_INPUT;

// @type PMINIDUMP_CALLBACK_OUTPUT
typedef struct _MINIDUMP_CALLBACK_OUTPUT *PMINIDUMP_CALLBACK_OUTPUT;

// @type PMINIDUMP_EXCEPTION_INFORMATION
typedef struct _MINIDUMP_EXCEPTION_INFORMATION *PMINIDUMP_EXCEPTION_INFORMATION;

// @type PMINIDUMP_USER_STREAM
typedef struct _MINIDUMP_USER_STREAM *PMINIDUMP_USER_STREAM;

// @type PMINIDUMP_USER_STREAM_INFORMATION
typedef struct _MINIDUMP_USER_STREAM_INFORMATION *PMINIDUMP_USER_STREAM_INFORMATION;

// @type POINT
typedef struct tagPOINT POINT;

// @type PRTL_CRITICAL_SECTION
typedef struct _RTL_CRITICAL_SECTION *PRTL_CRITICAL_SECTION;

// @type PRTL_CRITICAL_SECTION_DEBUG
typedef struct _RTL_CRITICAL_SECTION_DEBUG *PRTL_CRITICAL_SECTION_DEBUG;

// @type PSCOPETABLE_ENTRY
typedef struct _SCOPETABLE_ENTRY *PSCOPETABLE_ENTRY;

// @type PTHREAD_START_ROUTINE
typedef DWORD (__stdcall *PTHREAD_START_ROUTINE)(LPVOID lpThreadParameter);

// @type PVOID
typedef void *PVOID;

// @type PWCHAR
typedef WCHAR *PWCHAR;

// @type PadButton
enum PadButton
{
  Button_Invalid = 0xFFFFFFFF,
  Button1_A = 0x0,
  Button2_X = 0x1,
  Button3_Y = 0x2,
  Button4_B = 0x3,
  PadDown = 0x4,
  PadLeft = 0x5,
  PadUp = 0x6,
  PadRight = 0x7,
  Select = 0x8,
  Start = 0x9,
  ShoulderLeft1_LT = 0xA,
  ShoulderLeft2_LB = 0xB,
  ShoulderRight1_RT = 0xC,
  ShoulderRight2_RB = 0xD,
  StickLeft = 0xE,
  StickRight = 0xF,
  NbButtons = 0x10,
};

// @type Replicator
struct Replicator;

// @type SHORT
typedef __int16 SHORT;

// @type SIZE_T
typedef ULONG_PTR SIZE_T;

// @type SOCKET
typedef UINT_PTR SOCKET;

// @type SYM_TYPE
enum SYM_TYPE : __int32
{
  SymNone = 0x0,
  SymCoff = 0x1,
  SymCv = 0x2,
  SymPdb = 0x3,
  SymExport = 0x4,
  SymDeferred = 0x5,
  SymSym = 0x6,
  SymDia = 0x7,
  SymVirtual = 0x8,
  NumSymTypes = 0x9,
};

// @type SYSTEMTIME
typedef struct _SYSTEMTIME SYSTEMTIME;

// @type ThrowInfo
typedef const struct _s_ThrowInfo ThrowInfo;

// @type TryBlockMapEntry
struct TryBlockMapEntry
{
  int tryLow __dec __tabform(NODUPS,1);
  int tryHigh __dec __tabform(NODUPS,1);
  int catchHigh __dec __tabform(NODUPS,1);
  int nCatches __dec __tabform(NODUPS,1);
  _UNKNOWN *pHandlerArray;
};

// @type TypeDescriptor
#pragma pack(push, 4)
struct TypeDescriptor
{
  unsigned int hash;
  void *spare;
  char name[];
};
#pragma pack(pop)

// @type UCHAR
typedef unsigned __int8 UCHAR;

// @type UINT
typedef unsigned int UINT;

// @type UINT_PTR
typedef unsigned int UINT_PTR;

// @type ULONG
typedef unsigned int ULONG;

// @type ULONG32
typedef unsigned int ULONG32;

// @type ULONG64
typedef unsigned __int64 ULONG64;

// @type ULONGLONG
typedef unsigned __int64 ULONGLONG;

// @type ULONG_PTR
typedef unsigned int ULONG_PTR;

// @type USHORT
typedef unsigned __int16 USHORT;

// @type UnDecorator
struct UnDecorator;

// @type UnwindMapEntry
struct UnwindMapEntry
{
  int toState __dec __tabform(NODUPS,1);
  _UNKNOWN *action;
};

// @type VS_FIXEDFILEINFO
typedef struct tagVS_FIXEDFILEINFO VS_FIXEDFILEINFO;

// @type WCHAR
typedef wchar_t WCHAR;

// @type WNDCLASSEXA
typedef struct tagWNDCLASSEXA WNDCLASSEXA;

// @type WNDPROC
typedef LRESULT (__stdcall *WNDPROC)(HWND, UINT, WPARAM, LPARAM);

// @type WORD
typedef unsigned __int16 WORD;

// @type WPARAM
typedef UINT_PTR WPARAM;

// @type WSAData
struct WSAData
{
  WORD wVersion;
  WORD wHighVersion;
  char szDescription[257];
  char szSystemStatus[129];
  unsigned __int16 iMaxSockets;
  unsigned __int16 iMaxUdpDg;
  char *lpVendorInfo;
};

// @type XINPUT_CAPABILITIES
typedef struct _XINPUT_CAPABILITIES XINPUT_CAPABILITIES;

// @type XINPUT_GAMEPAD
typedef struct _XINPUT_GAMEPAD XINPUT_GAMEPAD;

// @type XINPUT_STATE
typedef struct _XINPUT_STATE XINPUT_STATE;

// @type XINPUT_VIBRATION
typedef struct _XINPUT_VIBRATION XINPUT_VIBRATION;

// @type _ACTIVATION_CONTEXT_STACK
struct _ACTIVATION_CONTEXT_STACK
{
  struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME *ActiveFrame;
  struct _LIST_ENTRY FrameListCache;
  unsigned int Flags;
  unsigned int NextCookieSequenceNumber;
  unsigned int StackId;
};

// @type _BY_HANDLE_FILE_INFORMATION
struct _BY_HANDLE_FILE_INFORMATION
{
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD dwVolumeSerialNumber;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
  DWORD nNumberOfLinks;
  DWORD nFileIndexHigh;
  DWORD nFileIndexLow;
};

// @type _CLIENT_ID
#pragma pack(push, 8)
struct _CLIENT_ID
{
  HANDLE UniqueProcess;
  HANDLE UniqueThread;
};
#pragma pack(pop)

// @type _CONTEXT
#pragma pack(push, 4)
struct _CONTEXT
{
  DWORD ContextFlags;
  DWORD Dr0;
  DWORD Dr1;
  DWORD Dr2;
  DWORD Dr3;
  DWORD Dr6;
  DWORD Dr7;
  FLOATING_SAVE_AREA FloatSave;
  DWORD SegGs;
  DWORD SegFs;
  DWORD SegEs;
  DWORD SegDs;
  DWORD Edi;
  DWORD Esi;
  DWORD Ebx;
  DWORD Edx;
  DWORD Ecx;
  DWORD Eax;
  DWORD Ebp;
  DWORD Eip;
  DWORD SegCs;
  DWORD EFlags;
  DWORD Esp;
  DWORD SegSs;
  BYTE ExtendedRegisters[512];
};
#pragma pack(pop)

// @type _CURDIR
struct _CURDIR
{
  struct _UNICODE_STRING DosPath;
  void *Handle;
};

// @type _CoreCrtNonSecureSearchSortCompareFunction
typedef int (__cdecl *_CoreCrtNonSecureSearchSortCompareFunction)(const void *, const void *);

// @type _DISPLAY_DEVICEA
struct _DISPLAY_DEVICEA
{
  DWORD cb;
  CHAR DeviceName[32];
  CHAR DeviceString[128];
  DWORD StateFlags;
  CHAR DeviceID[128];
  CHAR DeviceKey[128];
};

// @type _EH3_EXCEPTION_REGISTRATION
struct _EH3_EXCEPTION_REGISTRATION
{
  struct _EH3_EXCEPTION_REGISTRATION *Next;
  PVOID ExceptionHandler;
  PSCOPETABLE_ENTRY ScopeTable;
  DWORD TryLevel;
};

// @type _EH4_SCOPETABLE
struct _EH4_SCOPETABLE
{
  DWORD GSCookieOffset;
  DWORD GSCookieXOROffset;
  DWORD EHCookieOffset;
  DWORD EHCookieXOROffset;
  struct _EH4_SCOPETABLE_RECORD ScopeRecord[];
};

// @type _EH4_SCOPETABLE_RECORD
struct _EH4_SCOPETABLE_RECORD
{
  int EnclosingLevel;
  void *FilterFunc;
  void *HandlerFunc;
};

// @type _EXCEPTION_DISPOSITION
enum _EXCEPTION_DISPOSITION
{
  ExceptionContinueExecution = 0x0,
  ExceptionContinueSearch = 0x1,
  ExceptionNestedException = 0x2,
  ExceptionCollidedUnwind = 0x3,
};

// @type _EXCEPTION_POINTERS
struct _EXCEPTION_POINTERS
{
  PEXCEPTION_RECORD ExceptionRecord;
  PCONTEXT ContextRecord;
};

// @type _EXCEPTION_RECORD
struct _EXCEPTION_RECORD
{
  DWORD ExceptionCode;
  DWORD ExceptionFlags;
  struct _EXCEPTION_RECORD *ExceptionRecord;
  PVOID ExceptionAddress;
  DWORD NumberParameters;
  ULONG_PTR ExceptionInformation[15];
};

// @type _EXCEPTION_REGISTRATION_RECORD
struct _EXCEPTION_REGISTRATION_RECORD
{
  struct _EXCEPTION_REGISTRATION_RECORD *Next;
  PEXCEPTION_ROUTINE Handler;
};

// @type _FILETIME
struct _FILETIME
{
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
};

// @type _FLOATING_SAVE_AREA
struct _FLOATING_SAVE_AREA
{
  DWORD ControlWord;
  DWORD StatusWord;
  DWORD TagWord;
  DWORD ErrorOffset;
  DWORD ErrorSelector;
  DWORD DataOffset;
  DWORD DataSelector;
  BYTE RegisterArea[80];
  DWORD Spare0;
};

// @type _GDI_TEB_BATCH
struct _GDI_TEB_BATCH
{
  unsigned int Offset;
  unsigned int HDC;
  unsigned int Buffer[310];
};

// @type _GUID
struct _GUID
{
  unsigned int Data1;
  unsigned __int16 Data2;
  unsigned __int16 Data3;
  unsigned __int8 Data4[8];
};

// @type _IMAGEHLP_MODULE64
#pragma pack(push, 8)
struct _IMAGEHLP_MODULE64
{
  DWORD SizeOfStruct;
  DWORD64 BaseOfImage;
  DWORD ImageSize;
  DWORD TimeDateStamp;
  DWORD CheckSum;
  DWORD NumSyms;
  SYM_TYPE SymType;
  CHAR ModuleName[32];
  CHAR ImageName[256];
  CHAR LoadedImageName[256];
  CHAR LoadedPdbName[256];
  DWORD CVSig;
  CHAR CVData[780];
  DWORD PdbSig;
  GUID PdbSig70;
  DWORD PdbAge;
  BOOL PdbUnmatched;
  BOOL DbgUnmatched;
  BOOL LineNumbers;
  BOOL GlobalSymbols;
  BOOL TypeInfo;
  BOOL SourceIndexed;
  BOOL Publics;
  DWORD MachineType;
  DWORD Reserved;
};
#pragma pack(pop)

// @type _INTERNAL_IF_OPER_STATUS
typedef INTERNAL_IF_OPER_STATUS _INTERNAL_IF_OPER_STATUS;

// @type _LARGE_INTEGER
union _LARGE_INTEGER
{
  struct
  {
    DWORD LowPart;
    LONG HighPart;
  };
  struct _LARGE_INTEGER::$837407842DC9087486FDFA5FEB63B74E u;
  LONGLONG QuadPart;
};

// @type _LIST_ENTRY
struct _LIST_ENTRY
{
  struct _LIST_ENTRY *Flink;
  struct _LIST_ENTRY *Blink;
};

// @type _LocaleUpdate
struct _LocaleUpdate;

// @type _MEMORY_BASIC_INFORMATION
struct _MEMORY_BASIC_INFORMATION
{
  PVOID BaseAddress;
  PVOID AllocationBase;
  DWORD AllocationProtect;
  SIZE_T RegionSize;
  DWORD State;
  DWORD Protect;
  DWORD Type;
};

// @type _MIB_IFROW
struct _MIB_IFROW
{
  WCHAR wszName[256];
  IF_INDEX dwIndex;
  IFTYPE dwType;
  DWORD dwMtu;
  DWORD dwSpeed;
  DWORD dwPhysAddrLen;
  UCHAR bPhysAddr[8];
  DWORD dwAdminStatus;
  INTERNAL_IF_OPER_STATUS dwOperStatus;
  DWORD dwLastChange;
  DWORD dwInOctets;
  DWORD dwInUcastPkts;
  DWORD dwInNUcastPkts;
  DWORD dwInDiscards;
  DWORD dwInErrors;
  DWORD dwInUnknownProtos;
  DWORD dwOutOctets;
  DWORD dwOutUcastPkts;
  DWORD dwOutNUcastPkts;
  DWORD dwOutDiscards;
  DWORD dwOutErrors;
  DWORD dwOutQLen;
  DWORD dwDescrLen;
  UCHAR bDescr[256];
};

// @type _MINIDUMP_CALLBACK_INFORMATION
#pragma pack(push, 4)
struct _MINIDUMP_CALLBACK_INFORMATION
{
  MINIDUMP_CALLBACK_ROUTINE CallbackRoutine;
  PVOID CallbackParam;
};
#pragma pack(pop)

// @type _MINIDUMP_CALLBACK_INPUT
#pragma pack(push, 4)
struct _MINIDUMP_CALLBACK_INPUT
{
  ULONG ProcessId;
  HANDLE ProcessHandle;
  ULONG CallbackType;
  #pragma pack(push, 4)
  union
  {
    HRESULT Status;
    MINIDUMP_THREAD_CALLBACK Thread;
    MINIDUMP_THREAD_EX_CALLBACK ThreadEx;
    MINIDUMP_MODULE_CALLBACK Module;
    MINIDUMP_INCLUDE_THREAD_CALLBACK IncludeThread;
    MINIDUMP_INCLUDE_MODULE_CALLBACK IncludeModule;
    MINIDUMP_IO_CALLBACK Io;
    MINIDUMP_READ_MEMORY_FAILURE_CALLBACK ReadMemoryFailure;
    ULONG SecondaryFlags;
    MINIDUMP_VM_QUERY_CALLBACK VmQuery;
    MINIDUMP_VM_PRE_READ_CALLBACK VmPreRead;
    MINIDUMP_VM_POST_READ_CALLBACK VmPostRead;
  };
  #pragma pack(pop)
};
#pragma pack(pop)

// @type _MINIDUMP_CALLBACK_OUTPUT
#pragma pack(push, 4)
struct _MINIDUMP_CALLBACK_OUTPUT
{
  #pragma pack(push, 4)
  union
  {
    ULONG ModuleWriteFlags;
    ULONG ThreadWriteFlags;
    ULONG SecondaryFlags;
    #pragma pack(push, 4)
    struct
    {
      ULONG64 MemoryBase;
      ULONG MemorySize;
    };
    #pragma pack(pop)
    #pragma pack(push, 4)
    struct
    {
      BOOL CheckCancel;
      BOOL Cancel;
    };
    #pragma pack(pop)
    HANDLE Handle;
    #pragma pack(push, 4)
    struct
    {
      MINIDUMP_MEMORY_INFO VmRegion;
      BOOL Continue;
    };
    #pragma pack(pop)
    #pragma pack(push, 4)
    struct
    {
      HRESULT VmQueryStatus;
      MINIDUMP_MEMORY_INFO VmQueryResult;
    };
    #pragma pack(pop)
    #pragma pack(push, 4)
    struct
    {
      HRESULT VmReadStatus;
      ULONG VmReadBytesCompleted;
    };
    #pragma pack(pop)
    HRESULT Status;
  };
  #pragma pack(pop)
};
#pragma pack(pop)

// @type _MINIDUMP_EXCEPTION_INFORMATION
#pragma pack(push, 4)
struct _MINIDUMP_EXCEPTION_INFORMATION
{
  DWORD ThreadId;
  PEXCEPTION_POINTERS ExceptionPointers;
  BOOL ClientPointers;
};
#pragma pack(pop)

// @type _MINIDUMP_INCLUDE_MODULE_CALLBACK
#pragma pack(push, 4)
struct _MINIDUMP_INCLUDE_MODULE_CALLBACK
{
  ULONG64 BaseOfImage;
};
#pragma pack(pop)

// @type _MINIDUMP_INCLUDE_THREAD_CALLBACK
#pragma pack(push, 4)
struct _MINIDUMP_INCLUDE_THREAD_CALLBACK
{
  ULONG ThreadId;
};
#pragma pack(pop)

// @type _MINIDUMP_IO_CALLBACK
#pragma pack(push, 4)
struct _MINIDUMP_IO_CALLBACK
{
  HANDLE Handle;
  ULONG64 Offset;
  PVOID Buffer;
  ULONG BufferBytes;
};
#pragma pack(pop)

// @type _MINIDUMP_MEMORY_INFO
#pragma pack(push, 4)
struct _MINIDUMP_MEMORY_INFO
{
  ULONG64 BaseAddress;
  ULONG64 AllocationBase;
  ULONG32 AllocationProtect;
  ULONG32 __alignment1;
  ULONG64 RegionSize;
  ULONG32 State;
  ULONG32 Protect;
  ULONG32 Type;
  ULONG32 __alignment2;
};
#pragma pack(pop)

// @type _MINIDUMP_MODULE_CALLBACK
#pragma pack(push, 4)
struct _MINIDUMP_MODULE_CALLBACK
{
  PWCHAR FullPath;
  ULONG64 BaseOfImage;
  ULONG SizeOfImage;
  ULONG CheckSum;
  ULONG TimeDateStamp;
  VS_FIXEDFILEINFO VersionInfo;
  PVOID CvRecord;
  ULONG SizeOfCvRecord;
  PVOID MiscRecord;
  ULONG SizeOfMiscRecord;
};
#pragma pack(pop)

// @type _MINIDUMP_READ_MEMORY_FAILURE_CALLBACK
#pragma pack(push, 4)
struct _MINIDUMP_READ_MEMORY_FAILURE_CALLBACK
{
  ULONG64 Offset;
  ULONG Bytes;
  HRESULT FailureStatus;
};
#pragma pack(pop)

// @type _MINIDUMP_THREAD_CALLBACK
#pragma pack(push, 4)
struct _MINIDUMP_THREAD_CALLBACK
{
  ULONG ThreadId;
  HANDLE ThreadHandle;
  CONTEXT Context;
  ULONG SizeOfContext;
  ULONG64 StackBase;
  ULONG64 StackEnd;
};
#pragma pack(pop)

// @type _MINIDUMP_THREAD_EX_CALLBACK
#pragma pack(push, 4)
struct _MINIDUMP_THREAD_EX_CALLBACK
{
  ULONG ThreadId;
  HANDLE ThreadHandle;
  CONTEXT Context;
  ULONG SizeOfContext;
  ULONG64 StackBase;
  ULONG64 StackEnd;
  ULONG64 BackingStoreBase;
  ULONG64 BackingStoreEnd;
};
#pragma pack(pop)

// @type _MINIDUMP_TYPE
typedef MINIDUMP_TYPE _MINIDUMP_TYPE;

// @type _MINIDUMP_USER_STREAM
#pragma pack(push, 4)
struct _MINIDUMP_USER_STREAM
{
  ULONG32 Type;
  ULONG BufferSize;
  PVOID Buffer;
};
#pragma pack(pop)

// @type _MINIDUMP_USER_STREAM_INFORMATION
#pragma pack(push, 4)
struct _MINIDUMP_USER_STREAM_INFORMATION
{
  ULONG UserStreamCount;
  PMINIDUMP_USER_STREAM UserStreamArray;
};
#pragma pack(pop)

// @type _MINIDUMP_VM_POST_READ_CALLBACK
#pragma pack(push, 4)
struct _MINIDUMP_VM_POST_READ_CALLBACK
{
  ULONG64 Offset;
  PVOID Buffer;
  ULONG Size;
  ULONG Completed;
  HRESULT Status;
};
#pragma pack(pop)

// @type _MINIDUMP_VM_PRE_READ_CALLBACK
#pragma pack(push, 4)
struct _MINIDUMP_VM_PRE_READ_CALLBACK
{
  ULONG64 Offset;
  PVOID Buffer;
  ULONG Size;
};
#pragma pack(pop)

// @type _MINIDUMP_VM_QUERY_CALLBACK
#pragma pack(push, 4)
struct _MINIDUMP_VM_QUERY_CALLBACK
{
  ULONG64 Offset;
};
#pragma pack(pop)

// @type _NT_TIB
struct _NT_TIB
{
  struct _EXCEPTION_REGISTRATION_RECORD *ExceptionList;
  PVOID StackBase;
  PVOID StackLimit;
  PVOID SubSystemTib;
  union
  {
    PVOID FiberData;
    DWORD Version;
  };
  PVOID ArbitraryUserPointer;
  struct _NT_TIB *Self;
};

// @type _OSVERSIONINFOA
struct _OSVERSIONINFOA
{
  DWORD dwOSVersionInfoSize;
  DWORD dwMajorVersion;
  DWORD dwMinorVersion;
  DWORD dwBuildNumber;
  DWORD dwPlatformId;
  CHAR szCSDVersion[128];
};

// @type _OVERLAPPED
struct _OVERLAPPED
{
  ULONG_PTR Internal;
  ULONG_PTR InternalHigh;
  union
  {
    struct
    {
      DWORD Offset;
      DWORD OffsetHigh;
    };
    PVOID Pointer;
  };
  HANDLE hEvent;
};

// @type _PEB
struct _PEB
{
  uint8_t InheritedAddressSpace;
  uint8_t ReadImageFileExecOptions;
  uint8_t BeingDebugged;
  union
  {
    uint8_t BitField;
    struct
    {
      unsigned __int8 ImageUsesLargePages : 1;
      unsigned __int8 IsProtectedProcess : 1;
      unsigned __int8 IsLegacyProcess : 1;
      unsigned __int8 IsImageDynamicallyRelocated : 1;
      unsigned __int8 SkipPatchingUser32Forwarders : 1;
      unsigned __int8 SpareBits : 3;
    };
  };
  void *Mutant;
  void *ImageBaseAddress;
  struct _PEB_LDR_DATA *Ldr;
  struct _RTL_USER_PROCESS_PARAMETERS *ProcessParameters;
  void *SubSystemData;
  void *ProcessHeap;
  struct _RTL_CRITICAL_SECTION *FastPebLock;
  void *AtlThunkSListPtr;
  void *IFEOKey;
  union
  {
    unsigned int CrossProcessFlags;
    struct
    {
      unsigned __int32 ProcessInJob : 1;
      unsigned __int32 ProcessInitializing : 1;
      unsigned __int32 ProcessUsingVEH : 1;
      unsigned __int32 ProcessUsingVCH : 1;
      unsigned __int32 ProcessUsingFTH : 1;
      unsigned __int32 ReservedBits0 : 27;
    };
  };
  union
  {
    void *KernelCallbackTable;
    void *UserSharedInfoPtr;
  };
  unsigned int SystemReserved[1];
  unsigned int AtlThunkSListPtr32;
  void *ApiSetMap;
  unsigned int TlsExpansionCounter;
  void *TlsBitmap;
  unsigned int TlsBitmapBits[2];
  void *ReadOnlySharedMemoryBase;
  void *HotpatchInformation;
  void **ReadOnlyStaticServerData;
  void *AnsiCodePageData;
  void *OemCodePageData;
  void *UnicodeCaseTableData;
  unsigned int NumberOfProcessors;
  unsigned int NtGlobalFlag;
  union _LARGE_INTEGER CriticalSectionTimeout;
  unsigned int HeapSegmentReserve;
  unsigned int HeapSegmentCommit;
  unsigned int HeapDeCommitTotalFreeThreshold;
  unsigned int HeapDeCommitFreeBlockThreshold;
  unsigned int NumberOfHeaps;
  unsigned int MaximumNumberOfHeaps;
  void **ProcessHeaps;
  void *GdiSharedHandleTable;
  void *ProcessStarterHelper;
  unsigned int GdiDCAttributeList;
  struct _RTL_CRITICAL_SECTION *LoaderLock;
  unsigned int OSMajorVersion;
  unsigned int OSMinorVersion;
  uint16_t OSBuildNumber;
  uint16_t OSCSDVersion;
  unsigned int OSPlatformId;
  unsigned int ImageSubsystem;
  unsigned int ImageSubsystemMajorVersion;
  unsigned int ImageSubsystemMinorVersion;
  unsigned int ActiveProcessAffinityMask;
  unsigned int GdiHandleBuffer[34];
  void *PostProcessInitRoutine;
  void *TlsExpansionBitmap;
  unsigned int TlsExpansionBitmapBits[32];
  unsigned int SessionId;
  union _ULARGE_INTEGER AppCompatFlags;
  union _ULARGE_INTEGER AppCompatFlagsUser;
  void *pShimData;
  void *AppCompatInfo;
  struct _UNICODE_STRING CSDVersion;
  const struct _ACTIVATION_CONTEXT_DATA *ActivationContextData;
  struct _ASSEMBLY_STORAGE_MAP *ProcessAssemblyStorageMap;
  const struct _ACTIVATION_CONTEXT_DATA *SystemDefaultActivationContextData;
  struct _ASSEMBLY_STORAGE_MAP *SystemAssemblyStorageMap;
  unsigned int MinimumStackCommit;
  struct _FLS_CALLBACK_INFO *FlsCallback;
  struct _LIST_ENTRY FlsListHead;
  void *FlsBitmap;
  unsigned int FlsBitmapBits[4];
  unsigned int FlsHighIndex;
  void *WerRegistrationData;
  void *WerShipAssertPtr;
  void *pContextData;
  void *pImageHeaderHash;
  union
  {
    unsigned int TracingFlags;
    struct
    {
      unsigned __int32 HeapTracingEnabled : 1;
      unsigned __int32 CritSecTracingEnabled : 1;
      unsigned __int32 SpareTracingBits : 30;
    };
  };
};

// @type _PEB_LDR_DATA
struct _PEB_LDR_DATA
{
  unsigned int Length;
  uint8_t Initialized;
  void *SsHandle;
  struct _LIST_ENTRY InLoadOrderModuleList;
  struct _LIST_ENTRY InMemoryOrderModuleList;
  struct _LIST_ENTRY InInitializationOrderModuleList;
  void *EntryInProgress;
  uint8_t ShutdownInProgress;
  void *ShutdownThreadId;
};

// @type _PROCESSOR_NUMBER
struct _PROCESSOR_NUMBER
{
  WORD Group;
  BYTE Number;
  BYTE Reserved;
};

// @type _RTL_CRITICAL_SECTION
#pragma pack(push, 8)
struct _RTL_CRITICAL_SECTION
{
  PRTL_CRITICAL_SECTION_DEBUG DebugInfo;
  LONG LockCount;
  LONG RecursionCount;
  HANDLE OwningThread;
  HANDLE LockSemaphore;
  ULONG_PTR SpinCount;
};
#pragma pack(pop)

// @type _RTL_CRITICAL_SECTION_DEBUG
struct _RTL_CRITICAL_SECTION_DEBUG
{
  WORD Type;
  WORD CreatorBackTraceIndex;
  struct _RTL_CRITICAL_SECTION *CriticalSection;
  LIST_ENTRY ProcessLocksList;
  DWORD EntryCount;
  DWORD ContentionCount;
  DWORD Flags;
  WORD CreatorBackTraceIndexHigh;
  WORD SpareWORD;
};

// @type _RTL_DRIVE_LETTER_CURDIR
struct _RTL_DRIVE_LETTER_CURDIR
{
  uint16_t Flags;
  uint16_t Length;
  unsigned int TimeStamp;
  struct _STRING DosPath;
};

// @type _RTL_USER_PROCESS_PARAMETERS
struct _RTL_USER_PROCESS_PARAMETERS
{
  unsigned int MaximumLength;
  unsigned int Length;
  unsigned int Flags;
  unsigned int DebugFlags;
  void *ConsoleHandle;
  unsigned int ConsoleFlags;
  void *StandardInput;
  void *StandardOutput;
  void *StandardError;
  struct _CURDIR CurrentDirectory;
  struct _UNICODE_STRING DllPath;
  struct _UNICODE_STRING ImagePathName;
  struct _UNICODE_STRING CommandLine;
  void *Environment;
  unsigned int StartingX;
  unsigned int StartingY;
  unsigned int CountX;
  unsigned int CountY;
  unsigned int CountCharsX;
  unsigned int CountCharsY;
  unsigned int FillAttribute;
  unsigned int WindowFlags;
  unsigned int ShowWindowFlags;
  struct _UNICODE_STRING WindowTitle;
  struct _UNICODE_STRING DesktopInfo;
  struct _UNICODE_STRING ShellInfo;
  struct _UNICODE_STRING RuntimeData;
  struct _RTL_DRIVE_LETTER_CURDIR CurrentDirectores[32];
  volatile unsigned int EnvironmentSize;
  volatile unsigned int EnvironmentVersion;
};

// @type _SCOPETABLE_ENTRY
struct _SCOPETABLE_ENTRY
{
  int EnclosingLevel;
  void *FilterFunc;
  void *HandlerFunc;
};

// @type _SECURITY_ATTRIBUTES
struct _SECURITY_ATTRIBUTES
{
  DWORD nLength;
  LPVOID lpSecurityDescriptor;
  BOOL bInheritHandle;
};

// @type _STARTUPINFOA
struct _STARTUPINFOA
{
  DWORD cb;
  LPSTR lpReserved;
  LPSTR lpDesktop;
  LPSTR lpTitle;
  DWORD dwX;
  DWORD dwY;
  DWORD dwXSize;
  DWORD dwYSize;
  DWORD dwXCountChars;
  DWORD dwYCountChars;
  DWORD dwFillAttribute;
  DWORD dwFlags;
  WORD wShowWindow;
  WORD cbReserved2;
  LPBYTE lpReserved2;
  HANDLE hStdInput;
  HANDLE hStdOutput;
  HANDLE hStdError;
};

// @type _STRING
struct _STRING
{
  unsigned __int16 Length;
  unsigned __int16 MaximumLength;
  char *Buffer;
};

// @type _SYSTEMTIME
struct _SYSTEMTIME
{
  WORD wYear;
  WORD wMonth;
  WORD wDayOfWeek;
  WORD wDay;
  WORD wHour;
  WORD wMinute;
  WORD wSecond;
  WORD wMilliseconds;
};

// @type _SYSTEM_INFO
struct _SYSTEM_INFO
{
  union
  {
    DWORD dwOemId;
    struct
    {
      WORD wProcessorArchitecture;
      WORD wReserved;
    };
  };
  DWORD dwPageSize;
  LPVOID lpMinimumApplicationAddress;
  LPVOID lpMaximumApplicationAddress;
  DWORD_PTR dwActiveProcessorMask;
  DWORD dwNumberOfProcessors;
  DWORD dwProcessorType;
  DWORD dwAllocationGranularity;
  WORD wProcessorLevel;
  WORD wProcessorRevision;
};

// @type _TEB
struct _TEB
{
  struct _NT_TIB NtTib;
  void *EnvironmentPointer;
  struct _CLIENT_ID ClientId;
  void *ActiveRpcHandle;
  void *ThreadLocalStoragePointer;
  struct _PEB *ProcessEnvironmentBlock;
  unsigned int LastErrorValue;
  unsigned int CountOfOwnedCriticalSections;
  void *CsrClientThread;
  void *Win32ThreadInfo;
  unsigned int User32Reserved[26];
  unsigned int UserReserved[5];
  void *WOW32Reserved;
  unsigned int CurrentLocale;
  unsigned int FpSoftwareStatusRegister;
  void *SystemReserved1[54];
  int ExceptionCode;
  struct _ACTIVATION_CONTEXT_STACK *ActivationContextStackPointer;
  uint8_t SpareBytes[36];
  unsigned int TxFsContext;
  struct _GDI_TEB_BATCH GdiTebBatch;
  struct _CLIENT_ID RealClientId;
  void *GdiCachedProcessHandle;
  unsigned int GdiClientPID;
  unsigned int GdiClientTID;
  void *GdiThreadLocalInfo;
  unsigned int Win32ClientInfo[62];
  void *glDispatchTable[233];
  unsigned int glReserved1[29];
  void *glReserved2;
  void *glSectionInfo;
  void *glSection;
  void *glTable;
  void *glCurrentRC;
  void *glContext;
  unsigned int LastStatusValue;
  struct _UNICODE_STRING StaticUnicodeString;
  wchar_t StaticUnicodeBuffer[261];
  void *DeallocationStack;
  void *TlsSlots[64];
  struct _LIST_ENTRY TlsLinks;
  void *Vdm;
  void *ReservedForNtRpc;
  void *DbgSsReserved[2];
  unsigned int HardErrorMode;
  void *Instrumentation[9];
  struct _GUID ActivityId;
  void *SubProcessTag;
  void *EtwLocalData;
  void *EtwTraceData;
  void *WinSockData;
  unsigned int GdiBatchCount;
  union
  {
    struct _PROCESSOR_NUMBER CurrentIdealProcessor;
    unsigned int IdealProcessorValue;
    uint8_t ReservedPad0;
  };
  uint8_t ReservedPad1;
  uint8_t ReservedPad2;
  uint8_t IdealProcessor;
  unsigned int GuaranteedStackBytes;
  void *ReservedForPerf;
  void *ReservedForOle;
  unsigned int WaitingOnLoaderLock;
  void *SavedPriorityState;
  unsigned int SoftPatchPtr1;
  void *ThreadPoolData;
  void **TlsExpansionSlots;
  unsigned int MuiGeneration;
  unsigned int IsImpersonating;
  void *NlsCache;
  void *pShimData;
  unsigned int HeapVirtualAffinity;
  void *CurrentTransactionHandle;
  struct _TEB_ACTIVE_FRAME *ActiveFrame;
  void *FlsData;
  void *PreferredLanguages;
  void *UserPrefLanguages;
  void *MergedPrefLanguages;
  unsigned int MuiImpersonation;
  union
  {
    volatile uint16_t CrossTebFlags;
    struct
    {
      unsigned __int16 SpareCrossTebBits : 16;
    };
  };
  union
  {
    uint16_t SameTebFlags;
    struct
    {
      unsigned __int16 SafeThunkCall : 1;
      unsigned __int16 InDebugPrint : 1;
      unsigned __int16 HasFiberData : 1;
      unsigned __int16 SkipThreadAttach : 1;
      unsigned __int16 WerInShipAssertCode : 1;
      unsigned __int16 RanProcessInit : 1;
      unsigned __int16 ClonedThread : 1;
      unsigned __int16 SuppressDebugMsg : 1;
      unsigned __int16 DisableUserStackWalk : 1;
      unsigned __int16 RtlExceptionAttached : 1;
      unsigned __int16 InitialThread : 1;
      unsigned __int16 SpareSameTebBits : 5;
    };
  };
  void *TxnScopeEnterCallback;
  void *TxnScopeExitCallback;
  void *TxnScopeContext;
  unsigned int LockCount;
  unsigned int SpareUlong0;
  void *ResourceRetValue;
};

// @type _TIME_ZONE_INFORMATION
struct _TIME_ZONE_INFORMATION
{
  LONG Bias;
  WCHAR StandardName[32];
  SYSTEMTIME StandardDate;
  LONG StandardBias;
  WCHAR DaylightName[32];
  SYSTEMTIME DaylightDate;
  LONG DaylightBias;
};

// @type _ThrowInfo
typedef ThrowInfo _ThrowInfo;

// @type _ULARGE_INTEGER
union _ULARGE_INTEGER
{
  struct
  {
    DWORD LowPart;
    DWORD HighPart;
  };
  struct _ULARGE_INTEGER::$0354AA9C204208F00D0965D07BBE7FAC u;
  ULONGLONG QuadPart;
};

// @type _UNICODE_STRING
struct _UNICODE_STRING
{
  unsigned __int16 Length;
  unsigned __int16 MaximumLength;
  wchar_t *Buffer;
};

// @type _WIN32_FIND_DATAA
struct _WIN32_FIND_DATAA
{
  DWORD dwFileAttributes;
  FILETIME ftCreationTime;
  FILETIME ftLastAccessTime;
  FILETIME ftLastWriteTime;
  DWORD nFileSizeHigh;
  DWORD nFileSizeLow;
  DWORD dwReserved0;
  DWORD dwReserved1;
  CHAR cFileName[260];
  CHAR cAlternateFileName[14];
};

// @type _XINPUT_CAPABILITIES
struct _XINPUT_CAPABILITIES
{
  BYTE Type;
  BYTE SubType;
  WORD Flags;
  XINPUT_GAMEPAD Gamepad;
  XINPUT_VIBRATION Vibration;
};

// @type _XINPUT_GAMEPAD
struct _XINPUT_GAMEPAD
{
  WORD wButtons;
  BYTE bLeftTrigger;
  BYTE bRightTrigger;
  SHORT sThumbLX;
  SHORT sThumbLY;
  SHORT sThumbRX;
  SHORT sThumbRY;
};

// @type _XINPUT_STATE
struct _XINPUT_STATE
{
  DWORD dwPacketNumber;
  XINPUT_GAMEPAD Gamepad;
};

// @type _XINPUT_VIBRATION
struct _XINPUT_VIBRATION
{
  WORD wLeftMotorSpeed;
  WORD wRightMotorSpeed;
};

// @type __CT_flags
enum __bitmask __CT_flags : __int32
{
  CT_IsSimpleType = 0x1,
  CT_ByReferenceOnly = 0x2,
  CT_HasVirtualBase = 0x4,
  CT_IsWinRTHandle = 0x8,
  CT_IsStdBadAlloc = 0x10,
};

// @type __TI_flags
enum __bitmask __TI_flags : __int32
{
  TI_IsConst = 0x1,
  TI_IsVolatile = 0x2,
  TI_IsUnaligned = 0x4,
  TI_IsPure = 0x8,
  TI_IsWinRT = 0x10,
};

// @type __crt_locale_pointers
#pragma pack(push, 8)
struct __crt_locale_pointers
{
  struct __crt_locale_data *locinfo;
  struct __crt_multibyte_data *mbcinfo;
};
#pragma pack(pop)

// @type __m128
union __declspec(align(16)) __m128
{
  float m128_f32[4];
  unsigned __int64 m128_u64[2];
  __int8 m128_i8[16];
  __int16 m128_i16[8];
  __int32 m128_i32[4];
  __int64 m128_i64[2];
  unsigned __int8 m128_u8[16];
  unsigned __int16 m128_u16[8];
  unsigned __int32 m128_u32[4];
};

// @type __time32_t
typedef int __time32_t;

// @type __time64_t
typedef __int64 __time64_t;

// @type _beginthread_proc_type
typedef void (__cdecl *_beginthread_proc_type)(void *);

// @type _beginthreadex_proc_type
typedef unsigned int (__stdcall *_beginthreadex_proc_type)(void *);

// @type _complex
#pragma pack(push, 8)
struct _complex
{
  double x;
  double y;
};
#pragma pack(pop)

// @type _cpinfo
struct _cpinfo
{
  UINT MaxCharSize;
  BYTE DefaultChar[2];
  BYTE LeadByte[12];
};

// @type _crt_signal_t
typedef void (__cdecl *_crt_signal_t)(int);

// @type _dev_t
typedef unsigned int _dev_t;

// @type _ino_t
typedef unsigned __int16 _ino_t;

// @type _iobuf
struct _iobuf
{
  char *_ptr;
  int _cnt;
  char *_base;
  int _flag;
  int _file;
  int _charbuf;
  int _bufsiz;
  char *_tmpfname;
};

// @type _locale_t
typedef __crt_locale_pointers *_locale_t;

// @type _off_t
typedef int _off_t;

// @type _onexit_t
typedef int (__cdecl *_onexit_t)();

// @type _s_CatchableType
#pragma pack(push, 4)
struct _s_CatchableType
{
  unsigned int properties;
  TypeDescriptor *pType;
  PMD thisDisplacement;
  int sizeOrOffset;
  PMFN copyFunction;
};
#pragma pack(pop)

// @type _s_CatchableTypeArray
#pragma pack(push, 4)
struct _s_CatchableTypeArray
{
  int nCatchableTypes;
  CatchableType *arrayOfCatchableTypes[];
};
#pragma pack(pop)

// @type _s_ThrowInfo
#pragma pack(push, 4)
struct _s_ThrowInfo
{
  unsigned int attributes;
  PMFN pmfnUnwind;
  int (__cdecl *pForwardCompat)();
  CatchableTypeArray *pCatchableTypeArray;
};
#pragma pack(pop)

// @type _stat64
#pragma pack(push, 8)
struct _stat64
{
  _dev_t st_dev;
  _ino_t st_ino;
  unsigned __int16 st_mode;
  __int16 st_nlink;
  __int16 st_uid;
  __int16 st_gid;
  _dev_t st_rdev;
  __int64 st_size;
  __time64_t st_atime;
  __time64_t st_mtime;
  __time64_t st_ctime;
};
#pragma pack(pop)

// @type _stat64i32
#pragma pack(push, 8)
struct _stat64i32
{
  _dev_t st_dev;
  _ino_t st_ino;
  unsigned __int16 st_mode;
  __int16 st_nlink;
  __int16 st_uid;
  __int16 st_gid;
  _dev_t st_rdev;
  _off_t st_size;
  __time64_t st_atime;
  __time64_t st_mtime;
  __time64_t st_ctime;
};
#pragma pack(pop)

// @type charNode
struct charNode;

// @type exception
struct exception;

// @type fd_set
struct fd_set
{
  u_int fd_count;
  SOCKET fd_array[64];
};

// @type in_addr
struct in_addr
{
  union in_addr::$4F64C6817212FE4AF89030399F21EE2D S_un;
};

// @type pDNameNode
struct pDNameNode;

// @type pcharNode
struct pcharNode;

// @type rsize_t
typedef size_t rsize_t;

// @type size_t
typedef unsigned int size_t;

// @type sockaddr
struct sockaddr
{
  ADDRESS_FAMILY sa_family;
  CHAR sa_data[14];
};

// @type std::_System_error_category
struct std::_System_error_category;

// @type std::bad_alloc
struct std::bad_alloc;

// @type std::exception
struct std::exception;

// @type std::locale::facet
struct std::locale::facet;

// @type tagCURSORINFO
struct tagCURSORINFO
{
  DWORD cbSize;
  DWORD flags;
  HCURSOR hCursor;
  POINT ptScreenPos;
};

// @type tagHARDWAREINPUT
struct tagHARDWAREINPUT
{
  DWORD uMsg;
  WORD wParamL;
  WORD wParamH;
};

// @type tagINPUT
struct tagINPUT
{
  DWORD type;
  union
  {
    MOUSEINPUT mi;
    KEYBDINPUT ki;
    HARDWAREINPUT hi;
  };
};

// @type tagKEYBDINPUT
struct tagKEYBDINPUT
{
  WORD wVk;
  WORD wScan;
  DWORD dwFlags;
  DWORD time;
  ULONG_PTR dwExtraInfo;
};

// @type tagMOUSEINPUT
struct tagMOUSEINPUT
{
  LONG dx;
  LONG dy;
  DWORD mouseData;
  DWORD dwFlags;
  DWORD time;
  ULONG_PTR dwExtraInfo;
};

// @type tagMSG
struct tagMSG
{
  HWND hwnd;
  UINT message;
  WPARAM wParam;
  LPARAM lParam;
  DWORD time;
  POINT pt;
};

// @type tagPOINT
struct tagPOINT
{
  LONG x;
  LONG y;
};

// @type tagRECT
struct tagRECT
{
  LONG left;
  LONG top;
  LONG right;
  LONG bottom;
};

// @type tagVS_FIXEDFILEINFO
struct tagVS_FIXEDFILEINFO
{
  DWORD dwSignature;
  DWORD dwStrucVersion;
  DWORD dwFileVersionMS;
  DWORD dwFileVersionLS;
  DWORD dwProductVersionMS;
  DWORD dwProductVersionLS;
  DWORD dwFileFlagsMask;
  DWORD dwFileFlags;
  DWORD dwFileOS;
  DWORD dwFileType;
  DWORD dwFileSubtype;
  DWORD dwFileDateMS;
  DWORD dwFileDateLS;
};

// @type tagWNDCLASSEXA
struct tagWNDCLASSEXA
{
  UINT cbSize;
  UINT style;
  WNDPROC lpfnWndProc;
  int cbClsExtra;
  int cbWndExtra;
  HINSTANCE hInstance;
  HICON hIcon;
  HCURSOR hCursor;
  HBRUSH hbrBackground;
  LPCSTR lpszMenuName;
  LPCSTR lpszClassName;
  HICON hIconSm;
};

// @type timeval
struct timeval
{
  int tv_sec;
  int tv_usec;
};

// @type tm
#pragma pack(push, 8)
struct tm
{
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
};
#pragma pack(pop)

// @type type_info
struct type_info;

// @type u_int
typedef unsigned int u_int;

// @type u_long
typedef unsigned int u_long;

// @type u_short
typedef unsigned __int16 u_short;

// @type uint16_t
typedef unsigned __int16 uint16_t;

// @type uint8_t
typedef unsigned __int8 uint8_t;

// @type uintptr_t
typedef unsigned int uintptr_t;

// @type va_list
typedef char *va_list;

// @type wchar_t
typedef unsigned __int16 wchar_t;

// @type wctype_t
typedef unsigned __int16 wctype_t;

// @type wint_t
typedef unsigned __int16 wint_t;
