#ifndef PFSM_EXPLORATION_H
#define PFSM_EXPLORATION_H

#include <vector>
#include <stdio.h>
#include <iostream>
#include "controller.h"
#include "template_calculator.h"

using namespace std;

class pfsm_exploration: public Controller
{
  Template_Calculator t;
  uint moving_timer;
  int selected_action;
  uint timelim;
  float vmean;
  vector<vector<float>> policy;
  uint st; // current state
  float vx_ref, vy_ref;
public:
  pfsm_exploration();
  void action_motion(const int &selected_action, float r, float t, float &v_x, float &v_y);
  void state_action_lookup(const int ID, uint state_index);
  virtual void get_velocity_command(const uint8_t ID, float &v_x, float &v_y);
};

#endif /*PFSM_EXPLORATION_H*/