package edu.duke.raft;

import java.util.Timer;
import java.util.TimerTask;
import java.util.List;
import java.util.LinkedList;

public class CandidateMode extends RaftMode {
  //Election timer
  Timer electionTimer;
  //Vote count timer
  Timer voteCountTimer;

  public void go () {
    synchronized (mLock) {
      int term = mConfig.getCurrentTerm();      
      System.out.println ("S" + 
        mID + 
        "." + 
        term + 
        ": switched to candidate mode.");
      //Vote for yourself
      mConfig.setCurrentTerm(term, mID);
      //RaftResponses.init(mConfig.getNumServers(), term);
      //Update term for RaftResponses
      RaftResponses.setTerm(term);
      //Clear votes before an election
      RaftResponses.clearVotes(term);
      //System.out.println(mID + " raft term: " + RaftResponses.getTerm());
      //Initiate election timer
      electionTimer = scheduleTimer((long) (Math.random()*(ELECTION_TIMEOUT_MAX - ELECTION_TIMEOUT_MIN) + ELECTION_TIMEOUT_MIN), 1);
      //Send initial vote requests to other servers
      sendVoteRequests();
      //Initiate count vote timer
      //voteCountTimer = scheduleTimer((long) 5, 2);
    }
  }

  public void sendVoteRequests() {
    //Clear votes before an election
    //RaftResponses.clearVotes(term);
    //RaftResponses.setTerm(mConfig.getCurrentTerm());
    //Send vote requests to other servers (including itself)
    for(int i = 1; i <= mConfig.getNumServers(); i++) {
      this.remoteRequestVote(i, mConfig.getCurrentTerm(), mID, mLog.getLastIndex(), mLog.getLastTerm());
    }
    //Initiate count vote timer
    voteCountTimer = scheduleTimer((long) 5, 2);
  }

  public void countVotes() {
    //KINDA SKETCHY WAY TO AVOID NULL
    //RaftResponses.setTerm(mConfig.getCurrentTerm());
    //Check to see if a majority of the votes have been won
    int[] voteResponses = RaftResponses.getVotes(mConfig.getCurrentTerm());
    int numVotes = 0;
    if(voteResponses == null) {
      //System.out.println(mID + " voteResponses null");
      //System.out.println("cur " + mConfig.getCurrentTerm() + " RR: " + RaftResponses.getTerm());
    }
    else {
      for(int i = 1; i <= mConfig.getNumServers(); i++) {
        //If vote has not been received yet, resend request
        //if(voteResponses[i] == -1)
          //this.remoteRequestVote(i, mConfig.getCurrentTerm(), mID, mLog.getLastIndex(), mLog.getLastTerm());
        if(voteResponses[i] == 0)
          numVotes++;
      }
    }
    //If election is won, change to leader
    if(numVotes >= (mConfig.getNumServers()/2.0)) {
      electionTimer.cancel();
      voteCountTimer.cancel();
      RaftServerImpl.setMode(new LeaderMode());
    }
    else {
      voteCountTimer = scheduleTimer((long) 5, 2);
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
      //If candidate's term is greater than server's term, update server term and become follower
      if(candidateTerm > term) {
        //System.out.println(mID + " updated term to " + candidateTerm);
        mConfig.setCurrentTerm(candidateTerm, 0);
        electionTimer.cancel();
        voteCountTimer.cancel();
        RaftServerImpl.setMode(new FollowerMode());
        return term; //NECESSARY???
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
        //System.out.println(mID + " rejected " + leaderID + " in candidate");
        return term;
      }
      //If leader's term is greater than or equal to server's term, update server term
      if(leaderTerm >= term) {
        mConfig.setCurrentTerm(leaderTerm, 0);
        //Change to follower mode
        electionTimer.cancel();
        voteCountTimer.cancel();
        RaftServerImpl.setMode(new FollowerMode());
        //return 0; ??????SHOULD I REMOVE DIS
      }
      /*
      //Check if it is a hearbeat
      if(entries == null) {
        System.out.println("Hi");
        //Change to follower mode
        electionTimer.cancel();
        voteCountTimer.cancel();
        RaftServerImpl.setMode(new FollowerMode());
        return 0;
      }
      */
      //Handle non-heatbeat
      return term;
    }
  }

  // @param id of the timer that timed out
  public void handleTimeout (int timerID) {
    synchronized (mLock) {
      //Election timer case
      if(timerID == 1) {
        voteCountTimer.cancel();
        //Increment term
        mConfig.setCurrentTerm(mConfig.getCurrentTerm() + 1, 0);
        //Run go again to start a new election
        go();
      }
      //Vote count timer case
      if(timerID == 2) {
        //Count votes
        countVotes();
        //Reset vote count timer if server didn't win
        //voteCountTimer = scheduleTimer((long) 5, 2);
      }

    }
  }
}
