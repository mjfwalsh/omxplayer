#pragma once

enum ControlFlow {
  CONTINUE = -1,
//EXIT_SUCCESS = 0,
//EXIT_FAILURE = 2,
  PLAY_STOPPED = 3,
  CHANGE_FILE = 4,
  CHANGE_PLAYLIST_ITEM = 5,
  RUN_PLAY_LOOP = 6,
  END_PLAY = 7,
  END_PLAY_WITH_ERROR = 8,
  ABORT_PLAY = 9,
  SHUTDOWN = 10,
};

class DMessage;

enum ControlFlow handle_event(enum Action search_key, DMessage *m);
