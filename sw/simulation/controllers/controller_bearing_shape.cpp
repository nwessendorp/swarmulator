#include "controller_bearing_shape.h"
#include "agent.h"
#include "particle.h"
#include "main.h"
#include "randomgenerator.h"
#include "omniscient_observer.h"
#include "auxiliary.h"

#include <algorithm> // std::find

#define _ddes 1.0 // Desired equilibrium distance
#define _kr 0.1 // Repulsion gain
#define _ka 2 // Attraction gain
#define _v_adj 0.5 // Adjustment velocity

// The omniscient observer is used to simulate sensing the other agents.
OmniscientObserver *o = new OmniscientObserver();

Controller_Bearing_Shape::Controller_Bearing_Shape() : Controller()
{
  state_action_matrix.clear();
  terminalinfo ti;
  ifstream state_action_matrix_file("./conf/state_action_matrices/state_action_matrix_triangle9.txt");

  if (state_action_matrix_file.is_open()) {
    ti.info_msg("Opened state action matrix file.");

    // Collect the data inside the a stream, do this line by line
    while (!state_action_matrix_file.eof()) {
      string line;
      getline(state_action_matrix_file, line);
      stringstream stream_line(line);
      int buff[10] = {};
      int columns = 0;
      int state_index;
      bool index_checked = false;
      while (!stream_line.eof()) {
        if (!index_checked) {
          stream_line >> state_index;
          index_checked = true;
        } else {
          stream_line >> buff[columns];
          columns++;
        }
      }
      if (columns > 0) {
        vector<int> actions_index( begin(buff), begin(buff) + columns );
        state_action_matrix.insert( pair<int, vector<int>>(state_index, actions_index) );
      }
    }
    state_action_matrix_file.close();
  }
  else {
    ti.debug_msg("Unable to open state action matrix file.");
  }

}

float Controller_Bearing_Shape::f_attraction(float u, float b_eq)
{
  float w;
  if (!(abs(b_eq - M_PI / 4.0) < 0.1 || abs(b_eq - (3*M_PI/4.0)) < 0.1 ))
    w = log((_ddes / _kr - 1) / exp(-_ka * _ddes)) / _ka;
  else
    w = log((_ddes / _kr - 1) / exp(-_ka * sqrt(pow(_ddes, 2.0) + pow(_ddes, 2.0)))) / _ka;

  return 1 / (1 + exp(-_ka * (u - w)));
}

float Controller_Bearing_Shape::f_repulsion(float u) { return -_kr / u; }
float Controller_Bearing_Shape::f_extra(float u) {return 0;}

float Controller_Bearing_Shape::get_attraction_velocity(float u, float b_eq)
{
  return f_attraction(u, b_eq) + f_repulsion(u) + f_extra(u);;
}

void attractionmotion(const float &v_r, const float &v_b, float &v_x, float &v_y)
{
  v_x = v_r * cos(v_b);
  v_y = v_r * sin(v_b);
}

void latticemotion(const float &v_r, const float &v_adj, const float &v_b, const float &bdes, float &v_x, float &v_y)
{
  attractionmotion(v_r+v_adj, v_b, v_x, v_y);
  v_x += -v_adj * cos(bdes * 2 - v_b);
  v_y += -v_adj * sin(bdes * 2 - v_b);
}

void actionmotion(const int selected_action, float &v_x, float &v_y)
{
  int actionspace_y[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  int actionspace_x[8] = {1, 1, 0, -1, -1, -1, 0, 1};
  v_x = _v_adj * (float)actionspace_x[selected_action];
  v_y = _v_adj * (float)actionspace_y[selected_action];
}


bool Controller_Bearing_Shape::fill_template(vector<bool> &q, const float b_i, const float u, float dmax, float angle_err)
{
  vector<float> blink;

  // Angles to check for neighboring links
  blink.push_back(0);
  blink.push_back(M_PI / 4.0);
  blink.push_back(M_PI / 2.0);
  blink.push_back(3 * M_PI / 4.0);
  blink.push_back(M_PI);
  blink.push_back(deg2rad(180 + 45));
  blink.push_back(deg2rad(180 + 90));
  blink.push_back(deg2rad(180 + 135));
  blink.push_back(2 * M_PI);

  // Determine link (cycle through all options)
  if (u < dmax) {
    for (int j = 0; j < (int)blink.size(); j++) {
      if (abs(b_i - blink[j]) < deg2rad(angle_err)) {
        if (j == (int)blink.size() - 1) { // last element is back to 0
          q[0] = true;
        } else {
          q[j] = true;
        }
        return true;
      }
    }
  }
  return false;
}

float Controller_Bearing_Shape::get_preferred_bearing(const vector<float> &bdes, const float v_b)
{
  // Define in bv all equilibrium angles at which the agents can organize themselves
  vector<float> bv;
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < (int)bdes.size(); j++) {
      bv.push_back(bdes[j]);
    }
  }

  // Find what the desired angle is in bdes
  for (int i = 0; i < (int)bv.size(); i++) {
    if (i < (int)bdes.size() * 1) {
      bv[i] = abs(bv[i] - 2 * M_PI - v_b);
    } else if (i < (int)bdes.size() * 2) {
      bv[i] = abs(bv[i] - M_PI - v_b);
    } else if (i < (int)bdes.size() * 3) {
      bv[i] = abs(bv[i] - v_b);
    } else if (i < (int)bdes.size() * 4) {
      bv[i] = abs(bv[i] + M_PI - v_b);
    } else if (i < (int)bdes.size() * 5) {
      bv[i] = abs(bv[i] + 2 * M_PI - v_b);
    }
  }

  int minindex = 0;
  for (int i = 1; i < (int)bv.size(); i++) {
    if (bv[i] < bv[minindex]) {
      minindex = i;
    }
  }

  // Reduce the index for the angle of interest from bdes
  while (minindex >= (int)bdes.size()) {
    minindex -= (int)bdes.size();
  }

  // Returned the desired equilibrium bearing
  return bdes[minindex];
}

void Controller_Bearing_Shape::assess_situation(uint8_t ID, vector<bool> &q, vector<int> &q_ID, vector<bool> &q_precise)
{
  q.clear();
  q.assign(8,false);

  vector<int> closest = o->request_closest(ID); // Get vector of all neighbor IDs from closest to furthest

  // Fill the template with respect to the agent in question
  for (uint8_t i = 0; i < nagents - 1; i++) {
    if (fill_template(q, // Vector to fill
          wrapTo2Pi_f(o->request_bearing(ID, closest[i])), // Bearing
          o->request_distance(ID, closest[i]), // Distance
          _ddes * 1.7, 22.5)) { // Sensor range, bearing precision
            q_ID.push_back(closest[i]);
          }
  }

  for (uint8_t i = 0; i < nagents - 1; i++){
    fill_template(q_precise, // Vector to fill
      wrapTo2Pi_f(o->request_bearing(ID, closest[i])), // Bearing
      o->request_distance(ID, closest[i]), // Distance
      _ddes * 1.7, 15.0); // Sensor range, bearing precision
  }
}

vector<bool> moving(50, 0);
vector<int> moving_timer(50, 0);
vector<int> selected_action(50,-1);
vector<int> waiting_timer(50,0);
vector<int> state_index_store(50,0);

void Controller_Bearing_Shape::get_velocity_command(const uint8_t ID, float &v_x, float &v_y)
{
  v_x = 0;
  v_y = 0;
  
  vector<float> beta_des;
  beta_des.push_back(0.0);
  // beta_des.push_back(M_PI/4.0);
  beta_des.push_back(M_PI/2.0);
  // beta_des.push_back(3.0*M_PI/4.0);

  vector<int> closest = o->request_closest(ID); // Get vector of all neighbors from closest to furthest
  float v_b = wrapToPi_f(o->request_bearing(ID, closest[0]));
  float b_eq = get_preferred_bearing(beta_des, v_b);
  float v_r = get_attraction_velocity(o->request_distance(ID, closest[0]), b_eq);

  // State
  vector<bool> state(8, 0);
  vector<bool> state_precise(8, 0);
  vector<int>  state_ID;
  assess_situation(ID, state, state_ID, state_precise); // The ID is just used for simulation purposes
  int state_index = bool2int(state);
  int state_index_precise = bool2int(state_precise);

  // Print state
  cout << (int)ID << " q  = ";
  for (uint8_t i = 0; i < 8; i++){
    cout << state[i] << " ";
  }
  cout << endl << "  ID = ";
  for (uint8_t i = 0; i < state_ID.size(); i++)  {
    cout << state_ID[i] << " ";
  }
  cout << endl;


  // Can I move or are my neighbors moving?
  bool canImove = true;
  bool shouldImove = true;
  for (uint8_t i = 0; i < state_ID.size(); i++) {
    if (moving[state_ID[i]]) {
      // If someone is moving you can't move
      canImove = false;
    }
    if (!moving[ID] && o->request_distance(ID, closest[0]) < 0.9) //&& state_index != state_index_precise)
      shouldImove = false;
  }

  // Find if you are in a desired state
  // bool left_a_desired_state = false;
  vector<int> sdes = {3, 28, 31, 96, 124, 163, 190, 226, 227}; // todo: make this not a hack
  vector<float> priority = {5, 3, 4, 1, 2, 4, 3, 2, 3};          // todo: make this not a hack
  // vector<uint> sdes = { 3, 28, 96, 162 };
  // vector<uint> priority = {3, 2, 1, 2}; // todo: make this not a hack
  bool situationchanged = false;
  if (state_index != state_index_store[ID])
  {
    situationchanged = true;
    if (std::find(sdes.begin(), sdes.end(), state_index_store[ID]) == sdes.end() &&
        std::find(sdes.begin(), sdes.end(), state_index) != sdes.end())
    {
      cout << (int)ID << " entered a desired state! Now in state " << state_index << " from " << state_index_store[ID] << endl;
      int pos = std::find(sdes.begin(), sdes.end(), state_index) - sdes.begin();
      waiting_timer[ID] = 0;//1000 * pow(priority[pos]-1,2.0);
    }
  }
  state_index_store[ID] = state_index;


  // Try to find an action that suits the state, if available (otherwise you are in Sdes or Sblocked)
  // If you are already busy with an action, then don't change the action
  std::map<int, vector<int>>::iterator state_action_row;
  state_action_row = state_action_matrix.find(state_index);
  if ( (state_action_row != state_action_matrix.end() && !moving[ID]) && moving_timer[ID] < 70 ) {
    selected_action[ID] = *select_randomly(
      state_action_matrix.find(state_index)->second.begin(),
      state_action_matrix.find(state_index)->second.end());
  }
  else if (!moving[ID]) {
    selected_action[ID] = -2;
  }

  moving[ID] = false;
  float timelim = 200;
  if (selected_action[ID] > -1 && canImove && shouldImove && moving_timer[ID] < timelim && waiting_timer[ID] == 0 )
  {
    actionmotion(selected_action[ID], v_x, v_y);
    moving[ID] = true;
    moving_timer[ID]++;
  }
  else if (canImove) {
    // What a shame, you could move but you can't. Fix you position.
    latticemotion(v_r, _v_adj, v_b, b_eq, v_x, v_y);
    if (moving_timer[ID] >= 400)
      moving_timer[ID] = 0;
  }

  if (moving_timer[ID] >= timelim)
    moving_timer[ID]++;

  if (waiting_timer[ID] > 0)
    waiting_timer[ID]--;

  keepbounded(v_x, -1, 1);
  keepbounded(v_y, -1, 1);

}