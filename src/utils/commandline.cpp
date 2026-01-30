#include "irp/utils/commandline.h"
#include <sstream>

void commandline::set_instance_name(string to_parse)
{
 instance_name = to_parse;
}

void commandline::set_sortie_name(string to_parse) { sortie_name = to_parse; }

void commandline::set_BKS_name(string to_parse) { BKS_name = to_parse; }

void commandline::set_default_sorties_name(string to_parse)
{
 char caractere1 = '/';
 char caractere2 = '\\';
 int position = (int)to_parse.find_last_of(caractere1);
 int position2 = (int)to_parse.find_last_of(caractere2);
 if (position2 > position)
  position = position2;

 if (position != -1)
 {
  string directory = to_parse.substr(0, position + 1) + "tradeoff/";
  string filename = to_parse.substr(position + 1, to_parse.length() - position - 1-4);

  sortie_name = directory + "STsol-" + filename+ "_veh-" + std::to_string(nbVeh) + "_rou-" + std::to_string(rou) + "_seed-" + std::to_string(seed) + "_demandseed-" + std::to_string(demand_seed) + "_scenario-" + std::to_string(nb_scenario);
  BKS_name = directory + "STbks-" + filename+ "_veh-" + std::to_string(nbVeh) + "_rou-" + std::to_string(rou) + "_scenario-" + std::to_string(nb_scenario);
 }
 else
 {
  sortie_name = to_parse.substr(0, position + 1) + "STsol-" +
         to_parse.substr(position + 1, to_parse.length() - 1) +"-seed-"+to_parse.substr(seed)+"-veh-"+to_parse.substr(nbVeh)+"-rou-"+to_parse.substr(rou)+"-scenario-"+to_parse.substr(nb_scenario);
  BKS_name = "STbks-" + to_parse;
 }
}

// constructeur
commandline::commandline(int argc, char *argv[])
{
 bool isTime = false;
 bool isOutput = false;
 bool isBKS = false;

 cout << argc << endl;
 cout << string(argv[0]) << endl;

 if (argc % 2 != 0 || argc > 24 || argc < 2)
 {
  cout << "incorrect command line" << endl;
  command_ok = false;
 }
 else
 {
  // default values
  set_instance_name(string(argv[1]));
  
  cpu_time = 1200;
  seed = 0;
  type = 0;  // unknown
  nbCli = -1; // unknown
  nbVeh = -1; // unknown
  relax = -1; // unknown
  rou = -1;
  stockout = false;
  nb_scenario = 0;
  demand_seed = 0;
  iter = 0;
  control_day_1 = 0;
  num_threads = 0; // 0 means use default (hardware_concurrency)
  force_delivery_clients.clear(); // empty by default
  // afficher le lancement du programme :
  // for ( int i = 0 ; i < argc ; i ++ )
  // cout << string(argv[i]) << " " ;
  // cout << endl ;

  // parameters
  for (int i = 2; i < argc; i += 2)
  {
   if (string(argv[i]) == "-t")
    cpu_time = atoi(argv[i + 1]);
   else if (string(argv[i]) == "-sol")
    set_sortie_name(string(argv[i + 1]));
   else if (string(argv[i]) == "-bks")
    set_BKS_name(string(argv[i + 1]));
   else if (string(argv[i]) == "-seed")
    seed = atoi(argv[i + 1]);
   else if (string(argv[i]) == "-type")
    type = atoi(argv[i + 1]);
   else if (string(argv[i]) == "-relax")
    relax = atoi(argv[i + 1]);
   else if (string(argv[i]) == "-cli")
    nbCli = atoi(argv[i + 1]);
   else if (string(argv[i]) == "-veh")
    nbVeh = atoi(argv[i + 1]);
   else if (string(argv[i]) == "-stock"){
     rou = atoi(argv[i + 1]);
     stockout=true;
   }
   else if (string(argv[i]) == "-scenario")
    nb_scenario = atoi(argv[i + 1]);
   else if (string(argv[i]) == "-demandseed")
    demand_seed = atoi(argv[i + 1]);
   else if (string(argv[i]) == "-iter")
    iter = atoi(argv[i + 1]);
   else if (string(argv[i]) == "-control_day_1")
    control_day_1 = atoi(argv[i + 1]);
   else if (string(argv[i]) == "-threads")
    num_threads = atoi(argv[i + 1]);
   else if (string(argv[i]) == "-force_delivery_clients") {
    // Parse comma-separated client IDs: e.g., "1,2,3"
    string clients_str = string(argv[i + 1]);
    if (!clients_str.empty()) {
     stringstream ss(clients_str);
     string token;
     while (getline(ss, token, ',')) {
      if (!token.empty()) {
       force_delivery_clients.push_back(atoi(token.c_str()));
      }
     }
    }
   }
   else
   {
    cout << "Commande non reconnue : " << string(argv[i]) << endl;
    command_ok = false;
   }
  }
  set_default_sorties_name(string(argv[1]));
  command_ok = true;
 }
}

void commandline::set_debug_prams(string instance)
{
 bool isTime = false;
 bool isOutput = false;
 bool isBKS = false;

 set_instance_name(instance);
 set_default_sorties_name(instance);

 cpu_time = 1200;
 seed = 1000;
 type = 38; // unknown
 nbCli = -1; // unknown
 nbVeh = 2; // unknown
 relax = -1; // unknown

 command_ok = true;
}

// destructeur
commandline::~commandline() {}

// renvoie le chemin de l'instance

string commandline::get_path_to_instance() { return instance_name; }

string commandline::get_path_to_solution() { return sortie_name; }

string commandline::get_path_to_BKS() { return BKS_name; }

int commandline::get_type() { return type; }

int commandline::get_stockout() { return stockout; }

int commandline::get_rou() { return rou; }

int commandline::get_nb_scenario() { return nb_scenario; }

// renvoie le nombre de clients
int commandline::get_nbCli() { return nbCli; }

// renvoie le nombre de vehicules optimal connu
int commandline::get_nbVeh() { return nbVeh; }

// renvoie le temps cpu allou
int commandline::get_cpu_time() { return cpu_time; }

// renvoie la seed
int commandline::get_seed() { return seed; }

// renvoie la seed de la demande
int commandline::get_demand_seed() { return demand_seed; }

int commandline::get_iter() { return iter; }

int commandline::get_control_day_1() { return control_day_1; }

int commandline::get_num_threads() { return num_threads; }

vector<int> commandline::get_force_delivery_clients() { return force_delivery_clients; }

// dit si la ligne de commande est valide
bool commandline::is_valid() { return command_ok; }