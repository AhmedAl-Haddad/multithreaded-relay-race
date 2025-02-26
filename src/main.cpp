#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <array>
#include <random>
#include <condition_variable>
#include "cs_helper.hpp"

using namespace std;

const int NUM_TEAMS = 4;      // Number of teams in the race
const int NUM_MEMBERS = 4;    // Number of athletes in each team

// Team and competitor names for the Women’s 4x100 meter relay
array<string, 4> astrTeams = { "Jamaica", "United States", "Great Britain", "Switzerland" };
array<array<string, 4>, 4> aastrCompetitors = { {
    { "Briana Williams", "Elaine Thompson-Herah", "Shelly-Ann Fraser-Pryce", "Shericka Jackson" },
    { "Javianne Oliver", "Teahna Daniels", "Jenna Prandini", "Gabrielle Thomas" },
    { "Asha Philip", "Imani Lansiquot", "Dina Asher-Smith", "Daryll Neita" },
    { "Ajla Del Ponte", "Mujinga Kambundji", "Salomé Kora", "Riccarda Dietsche" }
} };

// Class to generate thread-safe random float values in a given range
class RandomTwister {
public:
    RandomTwister(float min, float max) : distribution(min, max) {} // Initialize with range
    float generate() {
        mtx.lock(); // Ensure thread safety
        float x = distribution(engine); // Generate random number
        mtx.unlock();
        return x;
    }
private:
    mt19937 engine{ random_device{}() }; // Random number generator engine
    uniform_real_distribution<float> distribution; // Uniform distribution for random numbers
    mutex mtx; // Mutex for thread safety
};

// Mutex for thread-safe printing
mutex print_mtx;
void thrd_print(const string& str) {
    lock_guard<mutex> lock(print_mtx); // Lock mutex for safe printing
    cout << str;
}

// Class to manage race synchronization and rules
class RaceReferee {
private:
    barrier all_threads_started; // Barrier to synchronize all threads at the start
    barrier go; // Barrier to synchronize all threads at the race start
    atomic<bool> winner; // Atomic flag to ensure only one winner
    array<bool, NUM_TEAMS> disqualifiedTeams; // Track disqualified teams
public:
    RaceReferee() :
        all_threads_started(1 + (NUM_TEAMS * NUM_MEMBERS)),
        go(1 + (NUM_TEAMS * NUM_MEMBERS)),
        winner(false),
        disqualifiedTeams({ false, false, false, false }) {
    }
    barrier& getStartBarrier() { return all_threads_started; } // Get start barrier
    barrier& getGoBarrier() { return go; } // Get go barrier
    bool isWinnerDeclared() { return winner; } // Check if winner is declared
    bool setWinner() { return !winner.exchange(true); } // Set winner atomically
    bool isTeamDisqualified(int TeamIndex) const { return disqualifiedTeams[TeamIndex]; } // Check if team is disqualified
    void disqualifyTeam(int TeamIndex, const string& TeamName) {
        disqualifiedTeams[TeamIndex] = true; // Mark team as disqualified
        thrd_print("\nTeam " + TeamName + " has been DISQUALIFIED for dropping the baton!\n");
    }
    bool batonDropped(RandomTwister& generator) {
        return generator.generate() < 10.2f; // 10% chance of dropping the baton
    }
};

// Function executed by each thread to simulate a 4x4x100m relay
void thd_runner_4x4x100m(Competitor& a, Competitor* pPrevA, RandomTwister& generator, int teamIndex, RaceReferee& racereferee) {
    thrd_print(a.getPerson() + " ready, "); // Print runner is ready
    racereferee.getStartBarrier().arrive_and_wait(); // Wait for all threads to be ready
    racereferee.getGoBarrier().arrive_and_wait(); // Wait for race to start
    if (pPrevA == NULL)  thrd_print(a.getPerson() + " started, "); // First runner starts
    else {
        {
            unique_lock<mutex> lock(pPrevA->mtx); // Lock previous runner's mutex
            pPrevA->baton.wait(lock, [&] {return pPrevA->bFinished;}); // Wait for baton
        }
        if (pPrevA != nullptr) {
            if (racereferee.batonDropped(generator)) { // Check if baton is dropped
                racereferee.disqualifyTeam(teamIndex, a.getTeamName()); // Disqualify team
            }
        }
        thrd_print(a.getPerson() + " (" + a.getTeamName() + ")" + " took the baton from " + pPrevA->getPerson() + " (" + pPrevA->getTeamName() + ")\n");
    }
    float fSprintDuration_seconds = generator.generate(); // Random sprint time
    this_thread::sleep_for(chrono::milliseconds(static_cast<int>(fSprintDuration_seconds * 1000))); // Simulate sprint
    a.setTime(fSprintDuration_seconds); // Record sprint time
    thrd_print("Leg " + to_string(a.numBatonExchanges()) + ": " + a.getPerson() + " ran in " + to_string(fSprintDuration_seconds) + " seconds. (" + a.getTeamName() + ")\n");
    if (a.numBatonExchanges() == NUM_MEMBERS) { // Check if all legs are completed
        if (racereferee.setWinner() && !racereferee.isTeamDisqualified(teamIndex)) { // Declare winner if not disqualified
            cout << "\n Team " << a.getTeamName() << " is the WINNER!" << endl;
        }
    }
}

int main() {
    RaceReferee raceReferee; // Create race referee object
    thread thread_competitor[NUM_TEAMS][NUM_MEMBERS]; // Threads for each runner
    Team aTeams[NUM_TEAMS]; // Teams in the race
    Competitor athlete[NUM_TEAMS][NUM_MEMBERS]; // Athletes in the race
    float afTeamTime_s[NUM_TEAMS]; // Total sprint times for each team
    RandomTwister randGen_sprint_time(10.0f, 12.0f); // Random sprint time generator
    cout << "Re-run of the women’s 4x4x100 meter relay at the Tokyo 2020 Olympics.\n" << std::endl;

    // Initialize teams and athletes
    for (int i = 0; i < NUM_TEAMS; ++i) {
        afTeamTime_s[i] = 0;
        aTeams[i].setTeam(astrTeams[i]); // Set team name
        for (int j = 0; j < NUM_MEMBERS; ++j) {
            athlete[i][j].set(aastrCompetitors[i][j], &(aTeams[i])); // Set athlete details
            if (j == 0)  thread_competitor[i][j] = thread(thd_runner_4x4x100m, ref(athlete[i][j]), nullptr, ref(randGen_sprint_time), i, ref(raceReferee)); // First runner
            else  thread_competitor[i][j] = thread(thd_runner_4x4x100m, ref(athlete[i][j]), &athlete[i][j - 1], ref(randGen_sprint_time), i, ref(raceReferee)); // Subsequent runners
        }
    }

    // Synchronize all threads at the start
    raceReferee.getStartBarrier().arrive_and_wait();
    thrd_print("\n\nThe race official raises her starting pistol...\n");

    // Simulate starter gun delay
    float fStarterGun_s = RandomTwister(3.0f, 5.0f).generate();
    this_thread::sleep_for(chrono::milliseconds(static_cast<int>(fStarterGun_s * 1000)));

    // Start the race
    raceReferee.getGoBarrier().arrive_and_wait();
    thrd_print("\nGO !\n\n");

    // Join all threads
    for (int i = 0; i < NUM_TEAMS; ++i) {
        for (int j = 0; j < NUM_MEMBERS; ++j) {
            if (thread_competitor[i][j].joinable()) { thread_competitor[i][j].join(); }
        }
    }

    // Print team results
    cout << "\n\nTEAM RESULTS" << endl;
    for (int i = 0; i < NUM_TEAMS; ++i) {
        if (raceReferee.isTeamDisqualified(i)) { cout << "Team " << astrTeams[i] << " has been Disqualified." << endl; }
        else { aTeams[i].printTimes(); }
    }
    cout << endl;
    return 0;
}
