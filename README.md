# bcwatch

Experimental bitcoin transaction gatherer. I'm still do not know what I pretend to do with this tool.
It shows on terminal and stores on json/csv/sqlite unconfirmed transactions broadcasted by [blockchain.info](https://blockchain.info)

## Compile

```
make
```

## Use

- Save transactions on CSV files:

```
./bcwatch /media/usb/bclog csv
```

This command will generate two csv files, `bclog_dst.csv` and `bclog_src.csv`. 
These files contains an incremental transaction id, a timestamp, the source or destination address and the transferred or received amount of bitcoins.

- Save transactions on a Sqlite3 database:

```
./bcwatch /media/usb/bclog sql
```

- Save transactions on json format:

```
./bcwatch /media/usb/bclog json
```

- Save transactions on json by default:

Just don't include the second argument:

```
./bcwatch /media/usb/bclog
```

## Libraries

[easywsclient](https://github.com/dhbaird/easywsclient)

[picojson](https://github.com/kazuho/picojson)

## SQLite DB schema

```
CREATE TABLE transactions(
    id INT PRIMARY KEY,
    timestamp DATETIME NOT NULL);
CREATE TABLE src_addrs(
    id INT PRIMARY KEY,
    tid INTEGER NOT NULL,
    addr DOUBLE NOT NULL,
    amount TEXT NOT NULL,
    FOREIGN KEY(tid) REFERENCES TRANSACTIONS(id));
CREATE TABLE dst_addrs(
    id INT PRIMARY KEY,
    tid INTEGER NOT NULL,
    addr DOUBLE NOT NULL,
    amount TEXT NOT NULL,
    FOREIGN KEY(tid) REFERENCES TRANSACTIONS(id));

```

## csv output example

```
298870237,SRC,1510054091,1Jrajadh416mRzGrghSTadN23WCpZj82XF,100000000
298870237,DST,1510054091,1Bt3tFCffsDhQQv3Xy2myZsed1ZryWDQQ3,16241282
298870237,DST,1510054091,37W73JFcBpAbrdHeJQzVnmrMy8Hea4Mrus,83747418
298870238,SRC,1510054091,14xL6eSF8ApA7CN5LnxwKnqyuLvCmmPJrw,7130500
```

## Json output example

```
{
    "tid": 000000001,
    "ts": 1509576297,
    "sources" : [ 
        {
            "addr": "xXHt8e8ZR",
            "val": 12431,
        },
        {
            "addr": "xXHt8e8ZR",
            "val": 12431,
        }
    ],
    "destinations" : [ 
        {
            "addr": "xXHt8e8ZR",
            "amount 12431,
        },
        {
            "addr": "xXHt8e8ZR",
            "amount 12431,
        }
    ]
}
```
## Json original transaction example

```
{
  "op" : "utx",
  "x" : {
    "lock_time" : 0,
    "ver" : 1,
    "size" : 372,
    "inputs" : [ {
      "sequence" : 4294967295,
      "prev_out" : {
        "spent" : true,
        "tx_index" : 297233228,
        "type" : 0,
        "addr" : "1PuxXHt8e8ZRopS42HHHMVktrLfwR7UAZS",
        "value" : 45919,
        "n" : 1,
        "script" : "76a914fb56beb20f6c5a6e73ecd518f9272780778500c988ac"
      },
      "script" : "47304402206f432ed1641badb001bbb1c6d59318b1bbfbf77a7983282809a04bd4bde21af802205085891c02c66274016efe46b6f6ce388475d76ede7489ed4a04279986b034c40121031a9500dc671bb831080d7bcf11e89c3c0fc686fd9971a39d657822fa566b8123"
    }, {
      "sequence" : 4294967295,
      "prev_out" : {
        "spent" : true,
        "tx_index" : 295751890,
        "type" : 0,
        "addr" : "1EQJ7LwY8SxHFugbBJGLVLBhLaXBmbQADf",
        "value" : 31890,
        "n" : 2,
        "script" : "76a91493017c080ee53466fcd4f423a15f88307a0caa1a88ac"
      },
      "script" : "47304402205cf5243f306523ed7dcd2d1a79526ed4e67bdb442a4cf4b87dcd2523d2cbf22b0220246cbaabb95688cf07444c92e0673081b056fe3e90b5bc1e7cb31892da3312020121030b14374f96db053c3dd3cda62df37875ea506117407017f9524208f5197ecde8"
    } ],
    "time" : 1509576297,
    "tx_index" : 297233751,
    "vin_sz" : 2,
    "hash" : "55374cde3f3ef94bfbfaca83be432d5ef0bc87174420a0f0e6b3e0e0a3fca901",
    "vout_sz" : 2,
    "relayed_by" : "127.0.0.1",
    "out" : [ {
      "spent" : false,
      "tx_index" : 297233751,
      "type" : 0,
      "addr" : "14oSdvyiw6rWCHA5dQ4pBgqMAkEa9U7u38",
      "value" : 967,
      "n" : 0,
      "script" : "76a91429b0e97cf02baaa71a4d3604158dce3b431cd1ce88ac"
    }, {
      "spent" : false,
      "tx_index" : 297233751,
      "type" : 0,
      "addr" : "1KFAexDC4poBPBgWEsbte3tUBTHAtim7gj",
      "value" : 60012,
      "n" : 1,
      "script" : "76a914c82025886ae11b6a85cd7a13b86c1d14d8f70bff88ac"
    } ]
  }
}

```
