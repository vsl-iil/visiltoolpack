typedef struct {
    DWORD count;
    DWORD highTime;
    DWORD lowTime;
} exceptionsState;

int OpenExclusions(HKEY* outKey);

int OpenExclSubkey(HKEY excl_key, const char* subkey, HKEY* hOutKey);

int QueryKey(HKEY excl_key, exceptionsState* outstate);
