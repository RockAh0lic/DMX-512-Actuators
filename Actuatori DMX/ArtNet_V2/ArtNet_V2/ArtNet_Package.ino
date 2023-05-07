void getDMXParams(int start_address, int params[6]) {
  new_command = false;
  int act_params[6] = { -1, -1, -1, -1, -1, -1};
  int j = 0;
  Udp.begin(localPort);
  int packetSize = Udp.parsePacket();
  //FIXME: test/debug check
  if (packetSize > art_net_header_size && packetSize <= max_packet_size) { //check size to avoid unneeded checks
    //if(packetSize) {

    IPAddress remote = Udp.remoteIP();
    remotePort = Udp.remotePort();
    Udp.read(packetBuffer, MAX_BUFFER_UDP);
    //read header
    match_artnet = 1;
    for (int i = 0; i < 7; i++) {
      //if not corresponding, this is not an artnet packet, so we stop reading
      if (packetBuffer[i] != ArtNetHead[i]) {
        match_artnet = 0; break;
      }
    }
    //if its an artnet header
    if (match_artnet == 1) {
      //artnet protocole revision, not really needed
      //is_artnet_version_1=packetBuffer[10];
      //is_artnet_version_2=packetBuffer[11];*/

      //sequence of data, to avoid lost packets on routeurs
      //seq_artnet=packetBuffer[12];*/

      //physical port of  dmx N°
      //artnet_physical=packetBuffer[13];*/

      //operator code enables to know wich type of message Art-Net it is
      Opcode = bytes_to_short(packetBuffer[9], packetBuffer[8]);
      //if opcode is DMX type
      if (Opcode == 0x5000) {
        is_opcode_is_dmx = 1; is_opcode_is_artpoll = 0;
      }

      //if opcode is artpoll
      else if (Opcode == 0x2000) {
        is_opcode_is_artpoll = 1; is_opcode_is_dmx = 0;
        //( we should normally reply to it, giving ip adress of the device)
      }

      //if its DMX data we will read it now
      if (is_opcode_is_dmx = 1) {

        //read incoming universe
        incoming_universe = bytes_to_short(packetBuffer[15], packetBuffer[14])
                            //if it is selected universe DMX will be read
        if (incoming_universe == select_universe) {

          //getting data from a channel position, on a precise amount of channels, this to avoid to much operation if you need only 4 channels for example
          //channel position
          for (int i = start_address; i < start_address + number_of_control_channels; i++) {
            buffer_channel_arduino[i - start_address] = byte(packetBuffer[i + art_net_header_size + 9]);
            act_params[j] = buffer_channel_arduino[i - start_address];
            j++;
          }
          for (int i = 0 ; i < 6; i++)
            params[i] = act_params[i];
        }
      }
    }//end of sniffing
    Udp.stop();
  }
}
