/* This program attempts to learn an objective function for a
   particular game by watching movies of people playing it. The
   objective function can then be used by playfun.exe
   to try to play the game.
 */

#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <map>

#include "tasbot.h"

#include "fceu/utils/md5.h"
#include "fceu/driver.h"
#include "fceu/drivers/common/args.h"
#include "fceu/state.h"
#include "basis-util.h"
#include "emulator.h"
#include "fceu/fceu.h"
#include "fceu/types.h"
#include "simplefm2.h"
#include "objective.h"
#include "weighted-objectives.h"
#include "motifs.h"

#ifdef MARIONET
#include "SDL.h"
#endif

// deprecated
#define FASTFORWARD 0

static void SaveMemory(vector< vector<uint8> > *memories) {
  memories->resize(memories->size() + 1);
  vector<uint8> *v = &memories->back();
  for (int i = 0; i < 0x800; i++) {
    v->push_back((uint8)RAM[i]);
  }
}

static vector< vector<int> > *objectives = NULL;
static void PrintAndSave(const vector<int> &ordering) {
  for (int i = 0; i < ordering.size(); i++) {
    printf("%d ", ordering[i]);
  }
  printf("\n");
  CHECK(objectives);
  objectives->push_back(ordering);
}

// With e.g. an divisor of 3, generate slices covering
// the first third, middle third, and last third.
static void GenerateNthSlices(int divisor, int num, 
			      const vector< vector<uint8> > &memories,
			      Objective *obj) {
  const int onenth = memories.size() / divisor;
  for (int slicenum = 0; slicenum < divisor; slicenum++) {
    vector<int> look;
    int low = slicenum * onenth;
    for (int i = 0; i < onenth; i++) {
      look.push_back(low + i);
    }
    printf("For slice %ld-%ld:\n", low, low + onenth - 1);
    for (int i = 0; i < num; i++) {
      obj->EnumerateFull(look, PrintAndSave, 1, slicenum * 0xBEAD + i);
    }
  }
}

static void GenerateOccasional(int stride, int offsets, int num,
			       const vector< vector<uint8> > &memories,
			       Objective *obj) {
  for (int off = 0; off < offsets; off++) {
    vector<int> look;
    // Consider starting at various places throughout the first stide?
    for (int start = off; start < memories.size(); start += stride) {
      look.push_back(start);
    }
    printf("For occasional @%d (every %d):\n", off, stride);
    for (int i = 0; i < num; i++) {
      obj->EnumerateFull(look, PrintAndSave, 1, off * 0xF00D + i);
    }
  }
}

static void MakeObjectives(const string &game,
			   const vector< vector<uint8> > &memories) {
  printf("Now generating objectives.\n");
  objectives = new vector< vector<int> >;
  Objective obj(memories);

  // Going to generate a bunch of objective functions.
  // Some things will never violate the objective, like
  // [world number, stage number] or [score]. So generate
  // a handful of whole-game objectives.

  // TODO: In Mario, all 50 appear to be effectively the same
  // when graphed. Are they all equivalent, and should we be
  // accounting for that e.g. in weighting or deduplication?
  for (int i = 0; i < 50; i++) // was 10
    obj.EnumerateFullAll(PrintAndSave, 1, i);

  // XXX Not sure how I feel about these, based on the
  // graphics. They are VERY noisy.

  // Next, generate objectives for each tenth of the game.
  GenerateNthSlices(10, 3, memories, &obj);

  // And for each 1/100th.
  // GenerateNthSlices(100, 1, memories, &obj);

  // Now, for individual frames spread throughout the
  // whole movie.
  // This one looks great.
  GenerateOccasional(100, 10, 10, memories, &obj);
  // was 5,2

  GenerateOccasional(250, 10, 10, memories, &obj);

  // This one looks okay; noisy at times.
  GenerateOccasional(1000, 10, 1, memories, &obj);

  // Weight them. Currently this is just removing duplicates.
  printf("There are %d objectives\n", objectives->size());
  WeightedObjectives weighted(*objectives);
  printf("And %d example memories\n", memories.size());
  weighted.WeightByExamples(memories);
  printf("And %d unique objectives\n", weighted.Size());

  weighted.SaveToFile(game + ".objectives");

  weighted.SaveSVG(memories, game + ".svg");
  weighted.SaveLua(6, game + ".lua");
}

int main(int argc, char *argv[]) {
  map<string, string> config = Util::ReadFileToMap("config.txt");
  if (config.empty()) {
    fprintf(stderr, "You need a file called config.txt; please "
	    "take a look at the README.\n");
    abort();
  }

  const string game = config["game"];
  const string moviename = config["movie"];

  CHECK(!game.empty());
  CHECK(!moviename.empty());

  Emulator::Initialize(game + ".nes");
  vector<uint8> movie = SimpleFM2::ReadInputs(moviename);
  CHECK(!movie.empty());

  vector< vector<uint8> > memories;
  memories.reserve(movie.size() + 1);
  vector<uint8> inputs;

  // The very beginning of most games start with RAM initialization,
  // which we really should ignore for building an objective function.
  // So skip until there's a button press in the movie.
  size_t start = 0;

  printf("Skipping frames without argument.\n");
  bool saw_input = false;
  while ((start < FASTFORWARD && !saw_input) && 
	 start < movie.size()) {
    if (movie[start] != 0) saw_input = true;
    Emulator::Step(movie[start]);
    start++;
  }

  CHECK(start != movie.size());

  printf("Skipped %ld frames until first keypress/ffwd.\n"
	 "Playing %ld frames...\n", start, movie.size() - start);

  SaveMemory(&memories);

  {
    vector<uint8> save;
    Emulator::SaveUncompressed(&save);
    printf("Save states are %ld bytes.\n", save.size());
  }

  uint64 time_start = time(NULL);
  for (int i = start; i < movie.size(); i++) {
    if (i % 1000 == 0) {
      printf("  [% 3.1f%%] %d/%ld\n", 
	     ((100.0 * i) / movie.size()), i, movie.size());
      // exit(0);
    }
    Emulator::Step(movie[i]);
    inputs.push_back(movie[i]);
    SaveMemory(&memories);
  }
  uint64 time_end = time(NULL);

  printf("Recorded %ld memories in %ld sec.\n", 
         memories.size(),
         time_end - time_start);

  MakeObjectives(game, memories);
  Motifs motifs;
  motifs.AddInputs(inputs);
  motifs.SaveToFile(game + ".motifs");

  Emulator::Shutdown();

  // exit the infrastructure
  FCEUI_Kill();
  return 0;
}
