#include "client.h"
#include "token.h"

void main() {
  shadey::ShadeyClient client(g_AuthToken, 2);
  client.run();
}