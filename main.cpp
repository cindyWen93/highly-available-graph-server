#include "mongoose.h"
#include "graph.h"
#include "GH.h"
#include<thread>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PlatformThreadFactory.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/TToString.h>
#include <boost/make_shared.hpp>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <thrift/transport/TBufferTransports.h>

#include <iostream>
#include <stdexcept>
#include <sstream>

using namespace std;
using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::concurrency;
using namespace apache::thrift::server;

using boost::shared_ptr;
using namespace GH;
using namespace std;

boost::shared_ptr<TTransport> graph_socket;
boost::shared_ptr<TTransport> graph_transport;
boost::shared_ptr<TProtocol> graph_protocol;
GHClient *graph_client;

int rpc_client_port = 9000;
int rpc_server_port = 9000;
static char * s_http_port = "8000";
static char * rpc_next_ip;

bool isLast = false;

static struct mg_serve_http_opts s_http_server_opts;


Graph graph;
//
class GHHandler : virtual public GHIf {
 public:
  GHHandler() {
    // Your initialization goes here
  }

  int32_t add_node(const int32_t node_id) {
    // Your implementation goes here
    if(isLast == false){
      cout << "RPC CALL SUCCESS\n";
      graph_transport->open();
      graph_client->add_node(node_id);
      graph_transport->close();
    }
    int ret = graph.add_node(node_id);
    printf("add_node\n");
    return ret;
  }

  int32_t add_edge(const int32_t node_a_id, const int32_t node_b_id) {
    // Your implementation goes here
    if(isLast == false){
      graph_transport->open();
      graph_client->add_edge(node_a_id,node_b_id);
      graph_transport->close();
    }

    int ret = graph.add_edge(node_a_id,node_b_id);
    printf("add_edge\n");
    return ret;
  }

  int32_t remove_node(const int32_t node_id) {
    // Your implementation goes here
    if(isLast == false){
      graph_transport->open();
      graph_client->remove_node(node_id);
      graph_transport->close();
    }

    int ret = graph.remove_node(node_id);
    printf("remove_node\n");
    return ret;
  }

  int32_t remove_edge(const int32_t node_a_id, const int32_t node_b_id) {
    // Your implementation goes here

    if(isLast == false){
      graph_transport->open();
      graph_client->remove_edge(node_a_id,node_b_id);
      graph_transport->close();
    }

    int ret = graph.remove_edge(node_a_id,node_b_id);
    printf("remove_edge\n");
    return ret;
  }

};

/* this function is used to get node_id from JSON field*/
const char* get_node(const char *p, int & node_id){
  string node;
  while(p!= NULL && (*p > '9' || *p <'0')){p++;}
  while(p!= NULL && (*p <= '9' && *p >= '0')){node.append(1,*(p++));}
  node_id = strtoll(node.c_str(), NULL, 10);
  return p;
}

static void handle_add_node_call(struct mg_connection *nc, struct http_message *hm) {

  const char *p = hm->body.p;
  int node_id; p = get_node(p,node_id);
  cout << "handle add node" << endl;
  if(isLast == false){
    graph_transport->open();
    graph_client->add_node(node_id);
    graph_transport->close();
  }
  int ret = graph.add_node(node_id);

  /* if the node already exists */
  if(ret == 204){
      mg_printf(nc, "%s", "HTTP/1.1 204 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
  }

  /* on success */
  else{
    mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
    mg_printf_http_chunk(nc, "{ \"node_id\": %d }", node_id);
  }
  mg_send_http_chunk(nc, "", 0);
}

static void handle_add_edge_call(struct mg_connection *nc, struct http_message *hm) {
  const char *p = hm->body.p;
  int node_a_id, node_b_id; p = get_node(p,node_a_id); p = get_node(p,node_b_id);
if(isLast == false){
  graph_transport->open();
  graph_client->add_edge(node_a_id,node_b_id);
  graph_transport->close();
}
  int ret = graph.add_edge(node_a_id,node_b_id);

  /* if either node doesn't exist, or if node_a_id is the same as node_b_id */
  if(ret == 400){
    mg_printf(nc, "%s", "HTTP/1.1 400 Bad Request\r\nTransfer-Encoding: chunked\r\n\r\n");
  }

  /* if the edge already exists */
  else if(ret == 204){
    mg_printf(nc, "%s", "HTTP/1.1 204 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
  }

  /* on success */
  else{
    mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
    mg_printf_http_chunk(nc, "{ \"node_a_id\": %d, ", node_a_id);
    mg_printf_http_chunk(nc, "\"node_b_id\": %d }", node_b_id);
  }
  mg_send_http_chunk(nc, "", 0);
}

static void handle_remove_node_call(struct mg_connection *nc, struct http_message *hm) {
  const char *p = hm->body.p;
  int node_id; p = get_node(p,node_id);
if(isLast == false){
  graph_transport->open();
  graph_client->remove_node(node_id);
  graph_transport->close();
}
  int ret = graph.remove_node(node_id);

  /* if the node does not exist */
  if(ret == 400){
      mg_printf(nc, "%s", "HTTP/1.1 400 Bad Request\r\nTransfer-Encoding: chunked\r\n\r\n");
  }

  /* on success */
  else{
    mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
  }
  mg_send_http_chunk(nc, "", 0);
}

static void handle_remove_edge_call(struct mg_connection *nc, struct http_message *hm) {
  const char *p = hm->body.p;
  int node_a_id, node_b_id; p = get_node(p,node_a_id); p = get_node(p,node_b_id);

if(isLast == false){
  graph_transport->open();
  graph_client->remove_edge(node_a_id,node_b_id);
  graph_transport->close();
}
  int ret = graph.remove_edge(node_a_id,node_b_id);

  /* if the edge does not exist */
  if(ret == 400){
      mg_printf(nc, "%s", "HTTP/1.1 400 Bad Request\r\nTransfer-Encoding: chunked\r\n\r\n");
  }

  /* on success */
  else{
    mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
  }
  mg_send_http_chunk(nc, "", 0);
}

static void handle_get_node_call(struct mg_connection *nc, struct http_message *hm) {
  const char *p = hm->body.p;
  int node_id; p = get_node(p,node_id);

  bool exists;
  graph.get_node(node_id,exists);

  /* on success*/
  mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
  mg_printf_http_chunk(nc, "{ \"in_graph\": %d }", exists);
  mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
}

static void handle_get_edge_call(struct mg_connection *nc, struct http_message *hm) {
  const char *p = hm->body.p;
  bool exists;
  int node_a_id, node_b_id; p = get_node(p,node_a_id); p = get_node(p,node_b_id);

  int ret = graph.get_edge(node_a_id,node_b_id,exists);

  /* at least one of the vertices does not exist */
  if(ret == 400){
    mg_printf(nc, "%s", "HTTP/1.1 400 Bad Request\r\nTransfer-Encoding: chunked\r\n\r\n");
  }

  /* on success*/
  else{
    mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
    mg_printf_http_chunk(nc, "{ \"in_graph\": %d }", exists);
  }
  mg_send_http_chunk(nc, "", 0);
}

static void handle_get_neighbors_call(struct mg_connection *nc, struct http_message *hm) {
  const char *p = hm->body.p;
  int node_id; p = get_node(p,node_id);
  vector<int> neighbors;

  int ret = graph.get_neighbors(node_id,neighbors);

  /* if the node does not exist*/
  if(ret == 400){
    mg_printf(nc, "%s", "HTTP/1.1 400 Bad Request\r\nTransfer-Encoding: chunked\r\n\r\n");
  }

  /* on success*/
  else{
    mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
    mg_printf_http_chunk(nc, "{ \"node_id\": %d,  \"neighbors\": [", node_id);
    for(int i = 0; i < neighbors.size(); i++){
      mg_printf_http_chunk(nc, "%d, ", neighbors[i]);
    }
    mg_printf_http_chunk(nc, "\t\t ] }");
  }
  mg_send_http_chunk(nc, "", 0); /* Send empty chunk, the end of response */
}

static void handle_shortest_path_call(struct mg_connection *nc, struct http_message *hm) {
  const char *p = hm->body.p;
  int node_a_id,node_b_id; p = get_node(p,node_a_id); p = get_node(p,node_b_id);
  int distance;

  int ret = graph.shortest_path(node_a_id,node_b_id,distance);

  /* if either node does not exist */
  if(ret == 400){
    mg_printf(nc, "%s", "HTTP/1.1 400 Bad Request\r\nTransfer-Encoding: chunked\r\n\r\n");
  }
  else if(ret == 204){
    mg_printf(nc, "%s", "HTTP/1.1 204 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
  }
  /* on success*/
  else{
      mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
      mg_printf_http_chunk(nc, "{ \"distance\": %d }", distance);
  }
  mg_send_http_chunk(nc, "", 0);
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
  struct http_message *hm = (struct http_message *) ev_data;

  switch (ev) {
    case MG_EV_HTTP_REQUEST:
      if (mg_vcmp(&hm->uri, "/api/v1/add_node") == 0) {
        handle_add_node_call(nc, hm);
      }
      else if (mg_vcmp(&hm->uri, "/api/v1/add_edge") == 0){
        handle_add_edge_call(nc, hm);
      }
      else if (mg_vcmp(&hm->uri, "/api/v1/remove_node") == 0){
        handle_remove_node_call(nc, hm);
      }
      else if (mg_vcmp(&hm->uri, "/api/v1/remove_edge") == 0){
        handle_remove_edge_call(nc, hm);
      }
      else if (mg_vcmp(&hm->uri, "/api/v1/get_node") == 0){
        handle_get_node_call(nc, hm);
      }
      else if (mg_vcmp(&hm->uri, "/api/v1/get_edge") == 0){
        handle_get_edge_call(nc, hm);
      }
      else if (mg_vcmp(&hm->uri, "/api/v1/get_neighbors") == 0){
        handle_get_neighbors_call(nc, hm);
      }
      else if (mg_vcmp(&hm->uri, "/api/v1/shortest_path") == 0){
        handle_shortest_path_call(nc, hm);
      }
      else{
        mg_send_http_chunk(nc, "", 0);
      }
      break;
    default:
          break;
  }
}


void rpc_listen(){
  boost::shared_ptr<GHHandler> handler(new GHHandler());
  boost::shared_ptr<TProcessor> processor(new GHProcessor(handler));
  boost::shared_ptr<TServerTransport> serverTransport(new TServerSocket(rpc_server_port));
  boost::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
  boost::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
  TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
  cout << "rpc listend" << endl;
  server.serve();
}

int main(int argc, char *argv[]) {
  struct mg_mgr mgr;
  struct mg_connection *nc;
  struct mg_bind_opts bind_opts;
  const char *err_str;

  int ch = getopt(argc,argv,"b");
  if(ch != 'b') {
    isLast = true;
    rpc_next_ip = "localhost";
    s_http_port = argv[1];
    rpc_server_port = atoi(argv[2]);
    rpc_client_port = atoi(argv[2]);
  }
  else{
    rpc_next_ip = argv[2];
    s_http_port = argv[3];
    rpc_server_port = atoi(argv[4]);
    rpc_client_port = atoi(argv[4]);
  }

  boost::thread nthread{rpc_listen};
  cout <<"RPC LISTEN ON PORT " << rpc_server_port<<endl;
  printf("NEXT NODE IS %s \n", rpc_next_ip);
  cout << isLast << endl;
  if(ch == 'b'){
    graph_socket = boost::shared_ptr<TTransport>(new TSocket(rpc_next_ip, rpc_client_port));
    graph_transport = boost::shared_ptr<TTransport>(new TBufferedTransport(graph_socket));
    graph_protocol = boost::shared_ptr<TProtocol>(new TBinaryProtocol(graph_transport));
    graph_client = new GHClient(graph_protocol);
  }

  mg_mgr_init(&mgr, NULL);

  /* Set HTTP server options */
  memset(&bind_opts, 0, sizeof(bind_opts));
  bind_opts.error_string = &err_str;

  nc = mg_bind_opt(&mgr, s_http_port, ev_handler, bind_opts);
  if (nc == NULL) {
    fprintf(stderr, "Error starting server on port %s: %s\n", s_http_port,
            *bind_opts.error_string);
    exit(1);
  }

  mg_set_protocol_http_websocket(nc);
  s_http_server_opts.enable_directory_listing = "yes";

  printf("Starting server on port %s, serving %s\n", s_http_port,
         s_http_server_opts.document_root);

  for (;;) {mg_mgr_poll(&mgr, 1000);}
  mg_mgr_free(&mgr);

  return 0;
}
