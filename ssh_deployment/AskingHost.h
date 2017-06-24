#include <string.h>

using namespace std;

class AskingHost {
public:
  AskingHost(string name) {fileName = name;}
  void testAvailableComputer(const char * , const char *);
private:
  string fileName;
};
