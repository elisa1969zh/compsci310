package edu.duke.raft;

import java.util.Timer;
import java.util.TimerTask;
import java.util.List;
import java.util.LinkedList;

public class FollowerMode extends RaftMode {
  //Election timer
  Timer electionTimer;

  public void go () {
    synchronized (mLock) {
      int term = mConfig.getCurrentTerm();
      System.out.println ("S" + 
        mID + 
        "." + 
        term + 
        ": switched to follower mode.");
      //Add election timer to see if leader failed
      electionTimer = scheduleTimer((long) (Math.random()*(ELECTION_TIMEOUT_MAX - ELECTION_TIMEOUT_MIN) + ELECTION_TIMEOUT_MIN), 1);
    }
  }
  
  // @param candidate’s term
  // @param candidate requesting vote
  // @param index of candidate’s last log entry
  // @param term of candidate’s last log entry
  // @return 0, if server votes for candidate; otherwise, server's
  // current term
  public int requestVote (int candidateTerm,
        int candidateID,
        int lastLogIndex,
        int lastLogTerm) {
    synchronized (mLock) {
      int term = mConfig.getCurrentTerm();
      //If candidate's term is greater than server's term, update server term
      if(candidateTerm > term) {
        mConfig.setCurrentTerm(candidateTerm, 0);
        term = mConfig.getCurrentTerm();
      }
      //If candidate's term is less than server's term, reject vote
      if(candidateTerm < term)
        return term;
      //If server did not vote for anyone or voted for the candidate
      //AND candidate's log is at least up to date as the servers, then approve vote
      if((mConfig.getVotedFor() == 0 || mConfig.getVotedFor() == candidateID)) {
        if(lastLogTerm > mLog.getLastTerm()) {
          electionTimer.cancel();
          electionTimer = scheduleTimer((long) (Math.random()*(ELECTION_TIMEOUT_MAX - ELECTION_TIMEOUT_MIN) + ELECTION_TIMEOUT_MIN), 1);
          mConfig.setCurrentTerm(term, candidateID);
          return 0;
        }
        if(lastLogTerm == mLog.getLastTerm()) {
          if(lastLogIndex >= mLog.getLastIndex()) {
            electionTimer.cancel();
            electionTimer = scheduleTimer((long) (Math.random()*(ELECTION_TIMEOUT_MAX - ELECTION_TIMEOUT_MIN) + ELECTION_TIMEOUT_MIN), 1);
            mConfig.setCurrentTerm(term, candidateID);
            return 0;
          }
        }
      }

      return term;
    }
  }
  

  // @param leader’s term
  // @param current leader
  // @param index of log entry before entries to append
  // @param term of log entry before entries to append
  // @param entries to append (in order of 0 to append.length-1)
  // @param index of highest committed entry
  // @return 0, if server appended entries; otherwise, server's
  // current term
  public int appendEntries (int leaderTerm,
          int leaderID,
          int prevLogIndex,
          int prevLogTerm,
          Entry[] entries,
          int leaderCommit) {
    synchronized (mLock) {
      int term = mConfig.getCurrentTerm();
      //If leader's term is less than server's term, reject append
      if(leaderTerm < term) {
        //System.out.println(mID + " rejected " + leaderID + " in follower");
        return term;
      }
      electionTimer.cancel();
      electionTimer = scheduleTimer((long) (Math.random()*(ELECTION_TIMEOUT_MAX - ELECTION_TIMEOUT_MIN) + ELECTION_TIMEOUT_MIN), 1);
      //If leader's term is greater than server's term, update server term
      if(leaderTerm > term)
        mConfig.setCurrentTerm(leaderTerm, 0);
      //Check if it is a hearbeat
      /*
      if(entries == null) {
        //System.out.println(mID + " recevied heartbeat");
        //Reset election timer
        electionTimer.cancel();
        electionTimer = scheduleTimer((long) (Math.random()*(ELECTION_TIMEOUT_MAX - ELECTION_TIMEOUT_MIN) + ELECTION_TIMEOUT_MIN), 1);
        return 0;
      }
      */
      //Handle non-heatbeat
      if(mLog.getEntry(prevLogIndex) == null) {
        return term;
      }
      if(mLog.getEntry(prevLogIndex).term != prevLogTerm) {
        return term;
      }
      mLog.insert(entries, prevLogIndex, prevLogTerm);
      if(leaderCommit > mCommitIndex) {
        mCommitIndex = leaderCommit;
      }

      return 0;
    }
  }  

  // @param id of the timer that timed out
  public void handleTimeout (int timerID) {
    synchronized (mLock) {
      //Election timer case
      if(timerID == 1) {
        //Increment term
        mConfig.setCurrentTerm(mConfig.getCurrentTerm() + 1, 0);
        //Change to candidate mode
        RaftServerImpl.setMode(new CandidateMode());
      }
    }
  }
}

