#include "GDBServer.h"
#include "utils.h"
#include "WCH_Regs.h"

//------------------------------------------------------------------------------
/*
At a minimum, a stub is required to support the ‘?’ command to tell GDB the
reason for halting, ‘g’ and ‘G’ commands for register access, and the ‘m’ and
‘M’ commands for memory access. Stubs that only control single-threaded targets
can implement run control with the ‘c’ (continue) command, and if the target
architecture supports hardware-assisted single-stepping, the ‘s’ (step) command.
Stubs that support multi-threading targets should support the ‘vCont’ command.
All other commands are optional.
*/

const GDBServer::handler GDBServer::handler_tab[] = {
  { "?",               &GDBServer::handle_questionmark },
  { "!",               &GDBServer::handle_bang },
  { "\003",            &GDBServer::handle_ctrlc },
  { "c",               &GDBServer::handle_c },
  { "D",               &GDBServer::handle_D },
  { "g",               &GDBServer::handle_g },
  { "H",               &GDBServer::handle_H },
  { "k",               &GDBServer::handle_k },
  { "m",               &GDBServer::handle_m },
  { "M",               &GDBServer::handle_M },
  { "p",               &GDBServer::handle_p },
  { "P",               &GDBServer::handle_P },
  { "qAttached",       &GDBServer::handle_qAttached },
  { "qC",              &GDBServer::handle_qC },
  { "qfThreadInfo",    &GDBServer::handle_qfThreadInfo },
  { "qL",              &GDBServer::handle_qL },
  { "qOffsets",        &GDBServer::handle_qOffsets },
  { "qsThreadInfo",    &GDBServer::handle_qsThreadInfo },
  { "qSupported",      &GDBServer::handle_qSupported },
  { "qSymbol",         &GDBServer::handle_qSymbol },
  { "qTfP",            &GDBServer::handle_qTfP },
  { "qTfV",            &GDBServer::handle_qTfV },
  { "qTStatus",        &GDBServer::handle_qTStatus },
  { "s",               &GDBServer::handle_s },
  { "vCont?",          &GDBServer::handle_vCont },
  { "vKill",           &GDBServer::handle_vKill },
  { "vMustReplyEmpty", &GDBServer::handle_vMustReplyEmpty },
};

const int GDBServer::handler_count = sizeof(GDBServer::handler_tab) / sizeof(GDBServer::handler_tab[0]);

//------------------------------------------------------------------------------

void GDBServer::put_byte(char b) {

  cb_put(b);

  if (!sending) {
    log("\n<< ");
    sending = true;
  }

  if (b) {
    log("%c", b);
  }
  else {
    log("\0");
  }
}

char GDBServer::get_byte() {
  auto b = cb_get();

  if (sending) {
    log("\n>> ");
    sending = false;
  }

  if (b) {
    log("%c", b);
  }
  else {
    log("\0");
  }

  /*
  if (b == '}') {
    return get_byte() ^ 0x20;
  }
  */

  return b;
}

//------------------------------------------------------------------------------
// Report why the CPU halted

void GDBServer::handle_questionmark() {
  //  SIGINT = 2
  send.set_packet("T02");
}

//----------
// Enable extended mode

void GDBServer::handle_bang() {
  send.set_packet("OK");
}

//----------
// Break

void GDBServer::handle_ctrlc() {
  send.set_packet("OK");
}

//----------
// Continue

void GDBServer::handle_c() {
  send.set_packet("");
}

//----------

void GDBServer::handle_D() {
  send.set_packet("OK");
}

//----------
// Read general registers

void GDBServer::handle_g() {
  send.start_packet();
  for (int i = 0; i < 16; i++) {
    send.put_u32(sl->get_gpr(i));
  }
  send.put_u32(sl->get_csr(CSR_DPC));
  send.end_packet();
}

//----------
// Write general registers

void GDBServer::handle_G() {
  /*
  packet_start();
  for (int i = 0; i < 32; i++) {
    packet_u32(0xF00D0000 + i);
  }
  packet_u32(0x00400020); // main() in basic
  packet_end();
  */

  send.set_packet("");
}

//----------
// Set thread for subsequent operations

void GDBServer::handle_H() {
  recv.skip_char('H');
  recv.skip(1);
  int thread_id = recv.get_hex();

  if (!recv.error) {
    send.set_packet("OK");
    return;
  }

  send.set_packet("E01");
}

//----------
// Kill

void GDBServer::handle_k() {
  // 'k' always kills the target and explicitly does _not_ have a reply.
}

//----------
// Read memory

void GDBServer::handle_m() {
  recv.skip_char('m');


  int addr = recv.get_hex();
  recv.skip_char(',');
  int size = recv.get_hex();

  if (recv.error) {
    log("\nhandle_m %x %x - recv.error '%s'\n", addr, size, recv.buf);
    send.set_packet("");
    return;
  }

  //log("\nhandle_m %x %x\n", addr, size);

  if (size == 4) {
    send.start_packet();
    send.put_u32(sl->get_mem(addr));
    send.end_packet();
  }
  else {
    uint32_t buf[256];
    sl->get_block(addr, buf, size/4);

    send.start_packet();
    for (int i = 0; i < size/4; i++) {
      send.put_u32(buf[i]);
    }
    send.end_packet();
  }
}

//----------
// Write memory

void GDBServer::handle_M() {
  // FIXME write data
  send.set_packet("");
}

//----------
// Read the value of register N

void GDBServer::handle_p() {
  recv.skip_char('p');
  int gpr = recv.get_hex();

  if (!recv.error) {
    send.start_packet();
    send.put_u32(sl->get_gpr(gpr));
    send.end_packet();
  }
}

//----------
// Write the value of register N

void GDBServer::handle_P() {
  send.set_packet("");
}

//----------
// Query what GDB is attached to

void GDBServer::handle_qAttached() {
  // -> qAttached
  // Reply:
  // ‘1’ The remote server attached to an existing process.
  // ‘0’ The remote server created a new process.
  // ‘E NN’ A badly formed request or an error was encountered.

  send.set_packet("1");
}

//----------
// Query thread ID

void GDBServer::handle_qC() {
  // -> qC
  // Return current thread ID
  // Reply: ‘QC<thread-id>’

  send.set_packet("QC0");
}

//----------
// Get section offsets that the target used when relocating the downloaded
// image. We don't have any offsets.

void GDBServer::handle_qOffsets() {
  send.set_packet("");
}

//----------
// Query thread information from RTOS

void GDBServer::handle_qL() {
  // FIXME get arg
  // qL1200000000000000000
  // qL 1 20 00000000 00000000
  // not sure if this is correct.
  send.set_packet("qM 01 1 00000000 00000001");
}

//----------
// Query trace status

void GDBServer::handle_qTStatus() {
  send.set_packet("T0");
}

//----------
// Query all active thread IDs

void GDBServer::handle_qfThreadInfo() {
  // Only one thread with id 1.
  send.set_packet("m0");
}

//----------
// Query all active thread IDs, continued

void GDBServer::handle_qsThreadInfo() {
  // Only one thread with id 1.
  send.set_packet("l");
}

//----------

void GDBServer::handle_qSupported() {
  // Tell GDB stuff we support (nothing)
  send.set_packet("");
}

//----------

void GDBServer::handle_qSymbol() {
  send.set_packet("OK");
}

//----------

void GDBServer::handle_qTfP() {
  send.set_packet("");
}

//----------

void GDBServer::handle_qTfV() {
  send.set_packet("");
}

//----------
// Step

void GDBServer::handle_s() {
  send.set_packet("");
}

//----------
// Resume, with different actions for each thread

void GDBServer::handle_vCont() {
  send.set_packet("");
}

//----------

void GDBServer::handle_vKill() {
  send.set_packet("OK");
}

//----------

void GDBServer::handle_vMustReplyEmpty() {
  send.set_packet("");
}

//------------------------------------------------------------------------------

void GDBServer::loop() {
  log("GDBServer::loop()\n");

  while(1) {

    //static int max_packets = 4;
    //static int packet_count = 0;

    //if (packet_count == max_packets) while(1);

    // Wait for start char
    while (get_byte() != '$');

    // Add bytes to packet until we see the end char
    char expected_checksum = 0;
    {
      recv.clear();
      while (1) {
        char c = get_byte();
        if (c == '#') {
          break;
        }
        else if (c == '}') {
          recv.put_buf(get_byte() ^ 0x20);
        }
        else {
          recv.put_buf(c);
        }
      }
      expected_checksum = (expected_checksum << 4) | from_hex(get_byte());
      expected_checksum = (expected_checksum << 4) | from_hex(get_byte());
    }

    if (recv.checksum != expected_checksum) {
      log("\n");
      log("Packet transmission error\n");
      log("expected checksum 0x%02x\n", expected_checksum);
      log("actual checksum   0x%02x\n", recv.checksum);
      put_byte('-');
      continue;
    }
    else {
      //log("\n");
      //log("packet checksum ok\n");
      //log("packet received %s\n", recv.buf);
      //log("packet size %d\n", recv.size);
      //log("packet cursor %d\n", recv.cursor);
      //log("packet sentinel1 %x\n", recv.sentinel1);
      //log("packet sentinel2 %x\n", recv.sentinel2);
    }

    // Packet checksum OK, handle it.
    put_byte('+');

#if 1
    recv.cursor = 0;

    handler_func h = nullptr;
    for (int i = 0; i < handler_count; i++) {
      if (cmp(handler_tab[i].name, recv.buf) == 0) {
        h = handler_tab[i].handler;
      }
    }

    if (h) {
      send.clear();
      (*this.*h)();
    }
    else {
      log("No handler for command %s\n", recv.buf);
      send.set_packet("");
    }

    if (!send.packet_valid) {
      log("Handler created a bad packet '%s'\n", send.buf);
      send.set_packet("");
    }

    while(1) {

      // The binary data representation uses 7d (ASCII ‘}’) as an escape character.
      // Any escaped byte is transmitted as the escape character followed by the
      // original character XORed with 0x20. For example, the byte 0x7d would be
      // transmitted as the two bytes 0x7d 0x5d. The bytes 0x23 (ASCII ‘#’), 0x24
      //(ASCII ‘$’), and 0x7d (ASCII ‘}’) must always be escaped. Responses sent by
      // the stub must also escape 0x2a (ASCII ‘*’), so that it is not interpreted
      // as the start of a run-length encoded sequence (described next).

      put_byte('$');
      for (int i = 0; i < send.size; i++) {
        char c = send.buf[i];
        if (c == '#' || c == '$' || c == '}' || c == '*') {
          put_byte('}');
          put_byte(c ^ 0x20);
        }
        else {
          put_byte(c);
        }
      }
      put_byte('#');
      put_byte(to_hex((send.checksum >> 4) & 0xF));
      put_byte(to_hex((send.checksum >> 0) & 0xF));


      char c = 0;
      do { c = get_byte(); }
      while (c != '+' && c != '-');

      if (c == '+') {
        break;
      }
      else {
        log("ack char %d '%c'\n", c, c);
      }

      //log("==============================\n");
      //log("==========  NACK %c ==========\n", c);
      //log("==============================\n");
    }
#endif

  }
}

//------------------------------------------------------------------------------
