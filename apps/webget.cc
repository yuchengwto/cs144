#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <sstream>


using namespace std;

void get_URL( const string& host, const string& path )
{
  std::stringstream ss;
  ss << "GET " << path << " HTTP/1.1\r\n"
     << "Host: " << host << "\r\n"
     << "Connection: close\r\n\r\n";
  auto req = ss.str();

  string buffer{};
  TCPSocket soc{};
  Address addr{host, "http"};
  soc.connect(addr);
  soc.write(req);


  std::stringstream resp;
  while (true) {
    soc.read(buffer);
    if (buffer.size() == 0) {
      break;
    }
    resp << buffer;
    buffer.clear();
  }
  auto resp_str = resp.str();
  auto pos = resp_str.find("\n\n");
  std::cout << resp_str.substr(pos+2);
  soc.close();
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}