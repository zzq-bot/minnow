#include "tcp_minnow_socket.hh"
#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

using namespace std;

void get_URL( const string& host, const string& path )
{
  // TCPSocket sock;
  CS144TCPSocket sock;
  Address addr { host, "http" }; // hostname and service
  sock.connect( addr );
  // follow 2.1: Fetch a Web page
  sock.write( "GET " + path + " HTTP/1.1\r\n" );
  sock.write( "Host: " + host + "\r\n" );
  sock.write( "Connection: close \r\n\r\n" ); // double enter here
  // Read into buffer: void read( std::string& buffer );

  string buffer;
  // sock.read( buf );
  // while ( 1 ) {
  // 之前用 if ( buf.empty() ) { 会出问题
  //   if ( sock.eof() ) {
  //     break;
  //   }
  //   cout << buf;
  //   sock.read( buf );
  // }

  while ( not sock.eof() ) {
    sock.read( buffer );
    cout << buffer;
  }
  sock.wait_until_closed();

  // cerr << "Function called: get_URL(" << host << ", " << path << ")\n";
  // cerr << "Warning: get_URL() has not been implemented yet.\n";
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
