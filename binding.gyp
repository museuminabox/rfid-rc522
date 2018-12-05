{
  "targets": [
    {
      "target_name": "rfid_rc522",
      "include_dirs" : [
      ],
      "sources": [
        "src/rc522.c",
        "src/rfid.c",
        "src/accessor.cc"
      ],
      "libraries": [
        "-lbcm2835"
      ]
    }
  ]
}
