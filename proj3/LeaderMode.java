package edu.duke.raft;

import java.util.Timer;
import java.util.TimerTask;
import java.util.List;
import java.util.LinkedList;

public class LeaderMode extends RaftMode {
  //Heartbeat timer
  Timer heartbeatTimer;
  //Append index
  int appendIndex;

  public void go () {
    synchronized (mLock) {
      int term = mConfig.getCurrentTerm();
      System.out.println ("S" + 
        mID + 
        "." + 
        term + 
        ": switched to leader mode.");
      //RaftResponses.init(mConfig.getNumServers(), term);
      //Update term for RaftResponses
      RaftResponses.setTerm(term);
      RaftResponses.clearAppendResponses(term);
      //Set append index to last index of server + 1
      appendIndex = mLog.getLastIndex();
      //Send inital heartbeats to other servers
      sendHeartbeats();
      //Initiate heartbeat timer
      heartbeatTimer = scheduleTimer((long) HEARTBEAT_INTERVAL, 1);
    }
  }
  
  public void sendHeartbeats() {
    //Send heartbeats to other servers
    for(int i = 1; i <= mConfig.getNumServers(); i++) {
      if(i != mID) {
        //System.out.println(mID + " sending heartbeat to " + i);
        this.remoteAppendEntries(i, mConfig.getCurrentTerm(), mID, appendIndex, mLog.getEntry(appendIndex).term, null, mCommitIndex);
      }
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
        heartbeatTimer.cancel();
        mConfig.setCurrentTerm(candidateTerm, 0);
        RaftServerImpl.setMode(new FollowerMode());
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
      //If another leader's term is greater, transition to follower
      if(leaderTerm > term) {
        heartbeatTimer.cancel();
        mConfig.setCurrentTerm(leaderTerm, 0);
        //System.out.println(mID + "got wrecked");
        RaftServerImpl.setMode(new FollowerMode());
        //return 0;
      }
      //System.out.println(mID + " rejected " + leaderID + " in leader ");
      //System.out.println("term: " + term + " leaderTerm: " + leaderTerm);
      return term;
    }
  }

  // @param id of the timer that timed out
  public void handleTimeout (int timerID) {
    synchronized (mLock) {
      //Heartbeat timer case
      if(timerID == 1) {
        //KINDA SKETCHY WAY TO AVOID NULL
        //RaftResponses.setTerm(mConfig.getCurrentTerm());
        //Check if any heartbeats got rejected
        int[] appendResponses = RaftResponses.getAppendResponses(mConfig.getCurrentTerm());
        if(appendResponses == null) {
          //System.out.println(mID + " heartbeatResponses null");
          //System.out.println("cur " + mConfig.getCurrentTerm() + " RR: " + RaftResponses.getTerm());
        }
        else {
          //Decrement appendIndex
          if(appendIndex > 0)
            appendIndex--;
          //Look through appendResponses
          for(int i = 1; i <= mConfig.getNumServers(); i++) {
            if(i != mID) {
              //System.out.println(appendIndex + " " + i + " " + appendResponses[i]);
              //If returned term is greater than server term, switch to follower
              if(appendResponses[i] > mConfig.getCurrentTerm()) {
                mConfig.setCurrentTerm(appendResponses[i], 0);
                RaftServerImpl.setMode(new FollowerMode());
                return;
              }
              //If returned term is 0, send heartbeat
              else if(appendResponses[i] == 0) {
                this.remoteAppendEntries(i, mConfig.getCurrentTerm(), mID, appendIndex, mLog.getEntry(appendIndex).term, null, mCommitIndex);
              }
              //Else do log repair
              else {
                //LinkedList<Entry> toAppend = new LinkedList<Entry>();
                Entry[] toAppend = new Entry[mLog.getLastIndex() - appendIndex];
                for(int j = appendIndex + 1; j <= mLog.getLastIndex(); j++) {
                  toAppend[j - appendIndex - 1] = mLog.getEntry(j);
                  //toAppend.add(mLog.getEntry(j));
                }
                this.remoteAppendEntries(i, mConfig.getCurrentTerm(), mID, appendIndex, mLog.getEntry(appendIndex).term, toAppend, mCommitIndex);
              }
            }
          }
        }
        //Send heartbeat
        //sendHeartbeats();
        //Reset heartbeat timer
        heartbeatTimer = scheduleTimer((long) HEARTBEAT_INTERVAL, 1);
      }
    }
  }
}
