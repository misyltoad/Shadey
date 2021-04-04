#include "client.h"
#include "token.h"

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;

  shadey::ShadeyClient client(g_AuthToken, 2);
  client.run();

  return 0;
}