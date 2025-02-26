#ifndef CS_ASSIGNMENT_FILE_HPP
#define CS_ASSIGNMENT_FILE_HPP

#include <stdio.h>
#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace std;

class barrier {
public:
    explicit barrier(std::size_t num_threads)
        : num_threads(num_threads), count(num_threads), generation(0) {}

    void arrive_and_wait() {
        std::unique_lock<std::mutex> lock(mtx);
        auto current_generation = generation;

        if (--count == 0) {
            generation++;  // Move to the next generation
            count = num_threads;  // Reset the count for the next barrier
            cv.notify_all();  // Wake up all threads
        } else {
            cv.wait(lock, [this, current_generation] { return current_generation != generation; });
        }
    }

private:
    std::mutex mtx;  
    std::condition_variable cv;
    std::size_t num_threads, count, generation;
};

class Team {
private:
    string teamName;
    float fTeamTime_s = 0;
    int iNumBatonExchanges = 0;
    std::mutex mtx;
public:
    Team() {;}
    Team(string tN) : teamName(tN) {}
    void setTeam(string tN) { teamName = tN; fTeamTime_s = 0; }
    string getTeam() { return teamName; }
    void addTime(float fCompetitorTime_s) {
        std::lock_guard<std::mutex> lock(mtx);
        fTeamTime_s += fCompetitorTime_s;
        iNumBatonExchanges++;
    }
    int numBatonExchanges() { return iNumBatonExchanges; }
    float getTime() { return fTeamTime_s; }
    void printTimes() {
        std::cout << "Team " << teamName << " = " << fTeamTime_s << " s" << std::endl;
    }
};

class Competitor {
private:
    string personName;
    Team *pTeam = NULL;
    float _fTime_s = 0;
public:
    std::condition_variable baton;
    std::mutex mtx;
    bool bFinished = false;
    Competitor() {;}
    Competitor(string pN, Team *pT) : personName(pN), pTeam(pT) {}
    void set(string pN, Team *pT) { personName = pN; pTeam = pT; }
    void setTime(float fT_s) {
        _fTime_s = fT_s;
        pTeam->addTime(fT_s);
        bFinished = true;
        std::lock_guard<std::mutex> lock(mtx);
        baton.notify_one();
    }
    float getTime() { return _fTime_s; }
    string getTeamName() { return pTeam->getTeam(); }
    void setPerson(string pN) { personName = pN; }
    string getPerson() { return personName; }
    int numBatonExchanges() { return pTeam->numBatonExchanges(); }
    void printCompetitor() {
        std::cout << "Competitor: Team= " << getTeamName() << ", Person = " << personName << ", Time = " << _fTime_s << " s" << std::endl;
    }
};

#endif