package edu.duke.raft;

/*
 * This class provides a centralized collection point for RPC
 * responses (i.e., requestVote and appendEntries). Responses are
 * collected under a given term. The term must be set to begin a
 * collection period, and incoming responses that do not match the
 * internal term will be ignored.
 *
 * Accesses to RaftResponses must be properly synchronized.
 */

public class RaftResponses {

  private static int[] mAppendResponses;
  private static int mTerm;
  private static int[] mVotes;

  // @param size of the network
  // @param current term
  public static void init (int size, int term) {
    mTerm = term;
    mVotes = new int[size + 1];
    clearVotes (term);
    mAppendResponses = new int[size + 1];    
    clearAppendResponses (term);
  }  

  // @param the current term
  public static void setTerm (int term) {
    mTerm = term;
  }

  // @param term for the election. 
  // @return null if the internal term not equal to the
  // paramter. array of voting results otherwise.
  public static int[] getVotes (int term) {
    if (term == mTerm) {
      return mVotes;
    }
    return null;
  }

  // @param term under which votes are being cleared. method has no
  // effect if the internal term is not equal to the paramter.
  // @return true if votes were cleared, false if not
  public static boolean clearVotes (int term) {
    if (term == mTerm) {
      for (int i=0; i<mVotes.length; i++) {
	mVotes[i] = -1;
      } 
      return true;
    }    
    return false;
  }  

  // @param server casting vote
  // @param return value from RPC to server
  // @param term for the election. method has no effect if the
  // internal term is not equal to the paramter.
  // @return true if vote was set, false if not
  public static boolean setVote (int serverID, int response, int term) {
    if (term == mTerm) {
      mVotes[serverID] = response;
      return true;
    }
    return false;
  }
  
  // @param term for the election. 
  // @return null if the internal term not equal to the
  // paramter. array of append responses otherwise.
  public static int[] getAppendResponses (int term) {
    if (term == mTerm) {
      return mAppendResponses;
    }    
    return null;
  }

  // @param term under which responses are being cleared. method has
  // no effect if the internal term is not equal to the paramter.
  // @return true if responses were cleared, false if not
  public static boolean clearAppendResponses (int term) {
    if (term == mTerm) {
      for (int i=0; i<mAppendResponses.length; i++) {
	mAppendResponses[i] = -1;
      }
      return true;
    }
    return false;
  }  
  
  // @param server casting vote
  // @param return value from RPC to server
  // @param term for the election. method has no effect if the
  // internal term is not equal to the paramter.
  // @return true if response was set, false if not
  public static boolean setAppendResponse (int serverID, 
					   int response, 
					   int term) {
    if (term == mTerm) {
      mAppendResponses[serverID] = response;
      return true;      
    } 
    return false;
  }
}

  
