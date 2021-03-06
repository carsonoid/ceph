// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */

#ifndef CEPH_MDSTABLESERVER_H
#define CEPH_MDSTABLESERVER_H

#include "MDSTable.h"

class MMDSTableRequest;

class MDSTableServer : public MDSTable {
public:
  int table;

  /* mds's requesting any pending ops.  child needs to encodig the corresponding
   * pending mutation state in the table.
   */
  struct _pending {
    uint64_t reqid;
    __s32 mds;
    version_t tid;
    void encode(bufferlist& bl) const {
      __u8 struct_v = 1;
      ::encode(struct_v, bl);
      ::encode(reqid, bl);
      ::encode(mds, bl);
      ::encode(tid, bl);
    }
    void decode(bufferlist::iterator& bl) {
      __u8 struct_v;
      ::decode(struct_v, bl);
      ::decode(reqid, bl);
      ::decode(mds, bl);
      ::decode(tid, bl);
    }
  };
  WRITE_CLASS_ENCODER(_pending)
  map<version_t,_pending> pending_for_mds;  // ** child should encode this! **


private:
  void handle_prepare(MMDSTableRequest *m);
  void _prepare_logged(MMDSTableRequest *m, version_t tid);
  struct C_Prepare : public Context {
    MDSTableServer *server;
    MMDSTableRequest *req;
    version_t tid;
    C_Prepare(MDSTableServer *s, MMDSTableRequest *r, version_t v) : server(s), req(r), tid(v) {}
    void finish(int r) {
      server->_prepare_logged(req, tid);
    }
  };

  void handle_commit(MMDSTableRequest *m);
  void _commit_logged(MMDSTableRequest *m);
  struct C_Commit : public Context {
    MDSTableServer *server;
    MMDSTableRequest *req;
    C_Commit(MDSTableServer *s, MMDSTableRequest *r) : server(s), req(r) {}
    void finish(int r) {
      server->_commit_logged(req);
    }
  };

  void handle_rollback(MMDSTableRequest *m);

 public:
  virtual void handle_query(MMDSTableRequest *m) = 0;
  virtual void _prepare(bufferlist &bl, uint64_t reqid, int bymds) = 0;
  virtual bool _commit(version_t tid, MMDSTableRequest *req=NULL) = 0;
  virtual void _rollback(version_t tid) = 0;
  virtual void _server_update(bufferlist& bl) { assert(0); }

  void _note_prepare(int mds, uint64_t reqid) {
    pending_for_mds[version].mds = mds;
    pending_for_mds[version].reqid = reqid;
    pending_for_mds[version].tid = version;
  }
  void _note_commit(uint64_t tid) {
    pending_for_mds.erase(tid);
  }
  void _note_rollback(uint64_t tid) {
    pending_for_mds.erase(tid);
  }
  

  MDSTableServer(MDS *m, int tab) : MDSTable(m, get_mdstable_name(tab), false), table(tab) {}
  virtual ~MDSTableServer() {}

  void handle_request(MMDSTableRequest *m);
  void do_server_update(bufferlist& bl);

  virtual void encode_server_state(bufferlist& bl) = 0;
  virtual void decode_server_state(bufferlist::iterator& bl) = 0;

  void encode_state(bufferlist& bl) {
    encode_server_state(bl);
    ::encode(pending_for_mds, bl);
  }
  void decode_state(bufferlist::iterator& bl) {
    decode_server_state(bl);
    ::decode(pending_for_mds, bl);
  }

  // recovery
  void finish_recovery();
  void handle_mds_recovery(int who);
};
WRITE_CLASS_ENCODER(MDSTableServer::_pending)

#endif
