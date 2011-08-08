/*
 * parse descriptor fields in transport- and program streams
 * Copyright (C) 1999-2004 Christian Wolff
 *
 * This program is free software; you can redistribute it and/or modify          * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or             * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License             * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ts2pesdescr.h"

char *iso639_1_code[136]={
  "aa","ab","af","am","ar","as","ay","az","ba","be","bg","bh",
  "bi","bn","bo","br","ca","co","cs","cy","da","de","dz","el",
  "en","eo","es","et","eu","fa","fi","fj","fo","fr","fy","ga",
  "gd","gl","gn","gu","ha","hi","hr","hu","hy","ia","ie","ik",
  "in","is","it","iw","ja","ji","jv","ka","kk","kl","km","kn",
  "ko","ks","ku","ky","la","ln","lo","lt","lv","mg","mi","mk",
  "ml","mn","mo","mr","ms","mt","my","na","ne","nl","no","oc",
  "om","or","pa","pl","ps","pt","qu","rm","rn","ro","ru","rw",
  "sa","sd","sg","sh","si","sk","sl","sm","sn","so","sq","sr",
  "ss","st","su","sv","sw","ta","te","tg","th","ti","tk","tl",
  "tn","to","tr","ts","tt","tw","uk","ur","uz","vi","vo","wo",
  "xh","yo","zh","zu"
};

char *iso639_1_name[136]={
  "Afar","Abkhazian","Afrikaans","Amharic",
  "Arabic","Assamese","Aymara","Azerbaijani",
  "Bashkir","Byelorussian","Bulgarian","Bihari",
  "Bislama","Bengali","Tibetan","Breton",
  "Catalan","Corsican","Czech","Welsh",
  "Danish","German","Bhutani","Greek",
  "English","Esperanto","Spanish","Estonian",
  "Basque","Persian","Finnish","Fiji",
  "Faroese","French","Frisian","Irish",
  "Scots Gaelic","Galician","Guarani","Gujarati",
  "Hausa","Hindi","Croatian","Hungarian",
  "Armenian","Interlingua","Interlingue","Inupiak",
  "Indonesian","Icelandic","Italian","Hebrew",
  "Japanese","Yiddish","Javanese","Georgian",
  "Kazakh","Greenlandic","Cambodian","Kannada",
  "Korean","Kashmiri","Kurdish","Kirghiz",
  "Latin","Lingala","Lao","Lithuanian",
  "Latvian, Lettish","Malagasy","Maori","Macedonian",
  "Malayalam","Mongolian","Moldavian","Marathi",
  "Malay","Maltese","Burmese","Nauru",
  "Nepali","Dutch","Norwegian","Occitan",
  "Oromo","Oriya","Punjabi","Polish",
  "Pashto, Pushto","Portuguese","Quechua","Rhaeto-Romance",
  "Kirundi","Romanian","Russian","Kinyarwanda",
  "Sanskrit","Sindhi","Sango","Serbo-Croatian",
  "Singhalese","Slovak","Slovenian","Samoan",
  "Shona","Somali","Albanian","Serbian",
  "Siswati","Sesotho","Sundanese","Swedish",
  "Swahili","Tamil","Telugu","Tajik",
  "Thai","Tigrinya","Turkmen","Tagalog",
  "Tswana","Tonga","Turkish","Tsonga",
  "Tatar","Twi","Ukrainian","Urdu",
  "Uzbek","Vietnamese","VolapŸk","Wolof",
  "Xhosa","Yoruba","Chinese","Zulu"
};

char *iso639_2t_code[432]={
  "aar","abk","ace","ach","ada","afa","afh","afr","ajm","aka","akk","ale",
  "alg","amh","ang","apa","ara","arc","arn","arp","art","arw","asm","ath",
  "aus","ava","ave","awa","aym","aze","bad","bai","bak","bal","bam","ban",
  "bas","bat","bej","bel","bem","ben","ber","bho","bih","bik","bin","bis",
  "bla","bnt","bod","bra","bre","btk","bua","bug","bul","cad","cai","car",
  "cat","cau","ceb","cel","ces","cha","chb","che","chg","chk","chm","chn",
  "cho","chp","chr","chu","chv","chy","cmc","cop","cor","cos","cpe","cpf",
  "cpp","cre","crp","cus","cym","dak","dan","day","del","den","deu","dgr",
  "din","div","doi","dra","dua","dum","dyu","dzo","efi","egy","eka","ell",
  "elx","eng","enm","epo","esp","est","eth","eus","ewe","ewo","fan","fao",
  "fas","fat","fij","fin","fiu","fon","fra","frm","fro","fry","ful","fur",
  "gaa","gai","gay","gba","gdh","gem","gez","gil","glg","gmh","goh","gon",
  "gor","got","grb","grc","grn","guj","gwi","hai","hau","haw","heb","her",
  "hil","him","hin","hit","hmn","hmo","hrv","hun","hup","hye","iba","ibo",
  "ijo","iku","ile","ilo","ina","inc","ind","ine","ipk","ira","iro","isl",
  "ita","jaw","jpn","jpr","jrb","kaa","kab","kac","kal","kam","kan","kar",
  "kas","kat","kau","kaw","kaz","kha","khi","khm","kho","kik","kin","kir",
  "kmb","kok","kom","kon","kor","kos","kpe","kro","kru","kua","kum","kur",
  "kut","lad","lah","lam","lao","lat","lav","lez","lin","lit","lol","loz",
  "ltz","lua","lub","lug","lui","lun","luo","lus","mad","mag","mah","mai",
  "mak","mal","man","map","mar","mas","max","mdr","men","mga","mic","min",
  "mis","mkd","mkh","mlg","mlt","mni","mno","moh","mol","mon","mos","mri",
  "msa","mul","mun","mus","mwr","mya","myn","nah","nai","nau","nav","nbl",
  "nde","ndo","nep","new","nia","nic","niu","nld","non","nor","nso","nub",
  "nya","nym","nyn","nyo","nzi","oci","oji","ori","orm","osa","oss","ota",
  "oto","paa","pag","pal","pam","pan","pap","pau","peo","phi","phn","pli",
  "pol","pon","por","pra","pro","pus","que","raj","rap","rar","roa","roh",
  "rom","ron","run","rus","sad","sag","sai","sal","sam","san","sas","sat",
  "sco","sel","sem","sga","shn","sid","sin","sio","sit","sla","slk","slv",
  "smi","smo","sna","snd","snk","sog","som","son","sot","spa","sqi","srd",
  "srp","srr","ssa","ssw","suk","sun","sus","sux","swa","swe","syr","tah",
  "tai","tam","tat","tel","tem","ter","tet","tgk","tgl","tha","tig","tir",
  "tiv","tkl","tli","tmh","tog","ton","tpi","tsi","tsn","tso","tuk","tum",
  "tur","tut","tvl","twi","tyv","uga","uig","ukr","umb","und","urd","uzb",
  "vai","ven","vie","vol","vot","wak","wal","war","was","wen","wol","xho",
  "yao","yap","yid","yor","ypk","zap","zen","zha","zho","znd","zul","zun"
};

char *iso639_2t_name[432]={
  "Afar","Abkhazian","Achinese","Acoli",
  "Adangme","Afro-Asiatic (Other)","Afrihili","Afrikaans",
  "Aljamia","Akan","Akkadian","Aleut",
  "Algonquian languages","Amharic","English, Old (ca. 450-1100)","Apache languages",
  "Arabic","Aramaic","Araucanian","Arapaho",
  "Artificial (Other)","Arawak","Assamese","Athapascan languages",
  "Australian languages","Avaric","Avestan","Awadhi",
  "Aymara","Azerbaijani","Banda","Bamileke languages",
  "Bashkir","Baluchi","Bambara","Balinese",
  "Basa","Baltic (Other)","Beja","Belarussian",
  "Bemba","Bengali","Berber (Other)","Bhojpuri",
  "Bihari","Bikol","Bini","Bislama",
  "Siksika","Bantu (Other)","Tibetan","Braj",
  "Breton","Batak (Indonesia)","Buriat","Buginese",
  "Bulgarian","Caddo","Central American Indian (Other)","Carib",
  "Catalan","Caucasian (Other)","Cebuano","Celtic (Other)",
  "Czech","Chamorro","Chibcha","Chechen",
  "Chagatai","Chuukese","Mari","Chinook jargon",
  "Choctaw","Chipewyan","Cherokee","Church Slavic",
  "Chuvash","Cheyenne","Chamic languages","Coptic",
  "Cornish","Corsican","Creoles and pidgins, English-based (Other)","Creoles and pidgins, French-based (Other)",
  "Creoles and pidgins, Portuguese-based (Other)","Cree","Creoles and pidgins (Other)","Cushitic (Other)",
  "Welsh","Dakota","Danish","Dayak",
  "Delaware","Slave (Athapascan)","German","Dogrib",
  "Dinka","Divehi","Dogri","Dravidian (Other)",
  "Duala","Dutch, Middle (ca. 1050-1350)","Dyula","Dzongkha",
  "Efik","Egyptian (Ancient)","Ekajuk","Greek, Modern (1453-)",
  "Elamite","English","English, Middle (1100-1500)","Esperanto",
  "Spanish","Estonian","Ethiopic","Basque",
  "Ewe","Ewondo","Fang","Faroese",
  "Persian","Fanti","Fijian","Finnish",
  "Finno-Ugrian (Other)","Fon","French","French, Middle (ca. 1400-1600)",
  "French, Old (ca. 842-1400)","Frisian","Fulah","Friulian",
  "Ga","Irish","Gayo","Gbaya",
  "Gaelic (Scots)","Germanic (Other)","Geez","Gilbertese",
  "Gallegan","German, Middle High (ca. 1050-1500)","German, Old High (ca. 750-1050)","Gondi",
  "Gorontalo","Gothic","Grebo","Greek, Ancient (to 1453)",
  "Guarani","Gujarati","Gwich'in","Haida",
  "Hausa","Hawaiian","Hebrew","Herero",
  "Hiligaynon","Himachali","Hindi","Hittite",
  "Hmong","Hiri Motu","Croatian","Hungarian",
  "Hupa","Armenian","Iban","Igbo",
  "Ijo","Inuktitut","Interlingue","Iloko",
  "Interlingua (International Auxilary Language Association)","Indic (Other)","Indonesian","Indo-European (Other)",
  "Inupiak","Iranian (Other)","Iroquoian languages","Icelandic",
  "Italian","Javanese","Japanese","Judeo-Persian",
  "Judeo-Arabic","Kara-Kalpak","Kabyle","Kachin",
  "Kalaallisut","Kamba","Kannada","Karen",
  "Kashmiri","Georgian","Kanuri","Kawi",
  "Kazakh","Khasi","Khoisan (Other)","Khmer",
  "Khotanese","Kikuyu","Kinyarwanda","Kirghiz",
  "Kimbundu","Konkani","Komi","Kongo",
  "Korean","Kosraean","Kpelle","Kru",
  "Kurukh","Kuanyama","Kumyk","Kurdish",
  "Kutenai","Ladino","Lahnda","Lamba",
  "Lao","Latin","Latvian","Lezghian",
  "Lingala","Lithuanian","Mongo","Lozi",
  "Letzeburgesch","Luba-Lulua","Luba-Katanga","Ganda",
  "Luiseno","Lunda","Luo (Kenya and Tanzania)","Lushai",
  "Madurese","Magahi","Marshall","Maithili",
  "Makasar","Malayalam","Mandingo","Austronesian (Other)",
  "Marathi","Masai","Manx","Mandar",
  "Mende","Irish, Middle (900-1200)","Micmac","Minangkabau",
  "Miscellaneous languages","Macedonian","Mon-Khmer (Other)","Malagasy",
  "Maltese","Manipuri","Manobo languages","Mohawk",
  "Moldavian","Mongolian","Mossi","Maori",
  "Malay","Multiple languages","Munda languages","Creek",
  "Marwari","Burmese","Mayan languages","Aztec",
  "North American Indian (Other)","Nauru","Navajo","Ndebele, South",
  "Ndebele, North","Ndonga","Nepali","Newari",
  "Nias","Niger-Kordofanian (Other)","Niuean","Dutch",
  "Norse, Old","Norwegian","Sohto, Northern ","Nubian languages",
  "Nyanja","Nyamwezi","Nyankole","Nyoro",
  "Nzima","Occitan (post 1500)","Ojibwa","Oriya",
  "Oromo","Osage","Ossetic","Turkish, Ottoman (1500-1928)",
  "Otomian languages","Papuan (Other)","Pangasinan","Pahlavi",
  "Pampanga","Panjabi","Papiamento","Palauan",
  "Persian, Old (ca. 600-400 B.C.)","Philippine (Other)","Phoenician","Pali",
  "Polish","Pohnpeian","Portuguese","Prakrit languages",
  "Provenc,al, Old (to 1500)","Pushto","Quechua","Rajasthani",
  "Rapanui","Rarotongan","Romance (Other)","Raeto-Romance",
  "Romany","Romanian","Rundi","Russian",
  "Sandawe","Sango","South American Indian (Other)","Salishan languages",
  "Samaritan Aramaic","Sanskrit","Sasak","Santali",
  "Scots","Selkup","Semitic (Other)","Irish, Old (to 900)",
  "Shan","Sidamo","Sinhalese","Siouan languages",
  "Sino-Tibetan (Other)","Slavic (Other)","Slovak","Slovenian",
  "S‡mi languages","Samoan","Shona","Sindhi",
  "Soninke","Sogdian","Somali","Songhai",
  "Sotho, Southern","Spanish","Albanian","Sardinian",
  "Serbian","Serer","Nilo-Saharan (Other)","Swati",
  "Sukuma","Sundanese","Susu","Sumerian",
  "Swahili","Swedish","Syriac","Tahitian",
  "Tai (Other)","Tamil","Tatar","Telugu",
  "Timne","Tereno","Tetum","Tajik",
  "Tagalog","Thai","Tigre","Tigrinya",
  "Tiv","Tokelau","Tlingit","Tamashek",
  "Tonga (Nyasa)","Tonga (Tonga Islands)","Tok Pisin","Tsimshian",
  "Tswana","Tsonga","Turkmen","Tumbuka",
  "Turkish","Altaic (Other)","Tuvalu","Twi",
  "Tuvinian","Ugaritic","Uighur","Ukrainian",
  "Umbundu","Undetermined","Urdu","Uzbek",
  "Vai","Venda","Vietnamese","VolapŸk",
  "Votic","Wakashan languages","Walamo","Waray",
  "Washo","Sorbian languages","Wolof","Xhosa",
  "Yao","Yapese","Yiddish","Yoruba",
  "Yupik languages","Zapotec","Zenaga","Zhuang",
  "Chinese","Zande","Zulu","Zu–i"
};

char *iso639_2b_code[432]={
  "aar","abk","ace","ach","ada","afa","afh","afr","ajm","aka","akk","alb",
  "ale","alg","amh","ang","apa","ara","arc","arm","arn","arp","art","arw",
  "asm","ath","aus","ava","ave","awa","aym","aze","bad","bai","bak","bal",
  "bam","ban","baq","bas","bat","bej","bel","bem","ben","ber","bho","bih",
  "bik","bin","bis","bla","bnt","bra","bre","btk","bua","bug","bul","bur",
  "cad","cai","car","cat","cau","ceb","cel","cha","chb","che","chg","chi",
  "chk","chm","chn","cho","chp","chr","chu","chv","chy","cmc","cop","cor",
  "cos","cpe","cpf","cpp","cre","crp","cus","cze","dak","dan","day","del",
  "den","dgr","din","div","doi","dra","dua","dum","dut","dyu","dzo","efi",
  "egy","eka","elx","eng","enm","epo","esp","est","eth","ewe","ewo","fan",
  "fao","fat","fij","fin","fiu","fon","fre","frm","fro","fry","ful","fur",
  "gaa","gae","gay","gba","gem","geo","ger","gez","gil","glg","gmh","goh",
  "gon","gor","got","grb","grc","gre","grn","guj","gwi","hai","hau","haw",
  "heb","her","hil","him","hin","hit","hmn","hmo","hun","hup","iba","ibo",
  "ice","ijo","iku","ile","ilo","ina","inc","ind","ine","ipk","ira","iri",
  "iro","ita","jav","jpn","jpr","jrb","kaa","kab","kac","kal","kam","kan",
  "kar","kas","kau","kaw","kaz","kha","khi","khm","kho","kik","kin","kir",
  "kmb","kok","kom","kon","kor","kos","kpe","kro","kru","kua","kum","kur",
  "kut","lad","lah","lam","lao","lat","lav","lez","lin","lit","lol","loz",
  "ltz","lua","lub","lug","lui","lun","luo","lus","mac","mad","mag","mah",
  "mai","mak","mal","man","mao","map","mar","mas","max","may","mdr","men",
  "mga","mic","min","mis","mkh","mlg","mlt","mni","mno","moh","mol","mon",
  "mos","mul","mun","mus","mwr","myn","nah","nai","nau","nav","nbl","nde",
  "ndo","nep","new","nia","nic","niu","non","nor","nso","nub","nya","nym",
  "nyn","nyo","nzi","oci","oji","ori","orm","osa","oss","ota","oto","paa",
  "pag","pal","pam","pan","pap","pau","peo","per","phi","phn","pli","pol",
  "pon","por","pra","pro","pus","que","raj","rap","rar","roa","roh","rom",
  "rum","run","rus","sad","sag","sai","sal","sam","san","sas","sat","scc",
  "sco","scr","sel","sem","sga","shn","sid","sin","sio","sit","sla","slo",
  "slv","smi","smo","sna","snd","snk","sog","som","son","sot","spa","srd",
  "srr","ssa","ssw","suk","sun","sus","sux","swa","swe","syr","tah","tai",
  "tam","tat","tel","tem","ter","tet","tgk","tgl","tha","tib","tig","tir",
  "tiv","tkl","tli","tmh","tog","ton","tpi","tsi","tsn","tso","tuk","tum",
  "tur","tut","tvl","twi","tyv","uga","uig","ukr","umb","und","urd","uzb",
  "vai","ven","vie","vol","vot","wak","wal","war","was","wel","wen","wol",
  "xho","yao","yap","yid","yor","ypk","zap","zen","zha","znd","zul","zun"
};

char *iso639_2b_name[432]={
  "Afar","Abkhazian","Achinese","Acoli",
  "Adangme","Afro-Asiatic (Other)","Afrihili","Afrikaans",
  "Aljamia","Akan","Akkadian","Albanian",
  "Aleut","Algonquian languages","Amharic","English, Old (ca. 450-1100)",
  "Apache languages","Arabic","Aramaic","Armenian",
  "Araucanian","Arapaho","Artificial (Other)","Arawak",
  "Assamese","Athapascan languages","Australian languages","Avaric",
  "Avestan","Awadhi","Aymara","Azerbaijani",
  "Banda","Bamileke languages","Bashkir","Baluchi",
  "Bambara","Balinese","Basque","Basa",
  "Baltic (Other)","Beja","Belarussian","Bemba",
  "Bengali","Berber (Other)","Bhojpuri","Bihari",
  "Bikol","Bini","Bislama","Siksika",
  "Bantu (Other)","Braj","Breton","Batak (Indonesia)",
  "Buriat","Buginese","Bulgarian","Burmese",
  "Caddo","Central American Indian (Other)","Carib","Catalan",
  "Caucasian (Other)","Cebuano","Celtic (Other)","Chamorro",
  "Chibcha","Chechen","Chagatai","Chinese",
  "Chuukese","Mari","Chinook jargon","Choctaw",
  "Chipewyan","Cherokee","Church Slavic","Chuvash",
  "Cheyenne","Chamic languages","Coptic","Cornish",
  "Corsican","Creoles and pidgins, English-based (Other)","Creoles and pidgins, French-based (Other)","Creoles and pidgins, Portuguese-based (Other)",
  "Cree","Creoles and pidgins (Other)","Cushitic (Other)","Czech",
  "Dakota","Danish","Dayak","Delaware",
  "Slave (Athapascan)","Dogrib","Dinka","Divehi",
  "Dogri","Dravidian (Other)","Duala","Dutch, Middle (ca. 1050-1350)",
  "Dutch","Dyula","Dzongkha","Efik",
  "Egyptian (Ancient)","Ekajuk","Elamite","English",
  "English, Middle (1100-1500)","Esperanto","Spanish","Estonian",
  "Ethiopic","Ewe","Ewondo","Fang",
  "Faroese","Fanti","Fijian","Finnish",
  "Finno-Ugrian (Other)","Fon","French","French, Middle (ca. 1400-1600)",
  "French, Old (ca. 842-1400)","Frisian","Fulah","Friulian",
  "Ga","Gaelic (Scots)","Gayo","Gbaya",
  "Germanic (Other)","Georgian","German","Geez",
  "Gilbertese","Gallegan","German, Middle High (ca. 1050-1500)","German, Old High (ca. 750-1050)",
  "Gondi","Gorontalo","Gothic","Grebo",
  "Greek, Ancient (to 1453)","Greek, Modern (1453-)","Guarani","Gujarati",
  "Gwich'in","Haida","Hausa","Hawaiian",
  "Hebrew","Herero","Hiligaynon","Himachali",
  "Hindi","Hittite","Hmong","Hiri Motu",
  "Hungarian","Hupa","Iban","Igbo",
  "Icelandic","Ijo","Inuktitut","Interlingue",
  "Iloko","Interlingua (International Auxilary Language Association)","Indic (Other)","Indonesian",
  "Indo-European (Other)","Inupiak","Iranian (Other)","Irish",
  "Iroquoian languages","Italian","Javanese","Japanese",
  "Judeo-Persian","Judeo-Arabic","Kara-Kalpak","Kabyle",
  "Kachin","Kalaallisut","Kamba","Kannada",
  "Karen","Kashmiri","Kanuri","Kawi",
  "Kazakh","Khasi","Khoisan (Other)","Khmer",
  "Khotanese","Kikuyu","Kinyarwanda","Kirghiz",
  "Kimbundu","Konkani","Komi","Kongo",
  "Korean","Kosraean","Kpelle","Kru",
  "Kurukh","Kuanyama","Kumyk","Kurdish",
  "Kutenai","Ladino","Lahnda","Lamba",
  "Lao","Latin","Latvian","Lezghian",
  "Lingala","Lithuanian","Mongo","Lozi",
  "Letzeburgesch","Luba-Lulua","Luba-Katanga","Ganda",
  "Luiseno","Lunda","Luo (Kenya and Tanzania)","Lushai",
  "Macedonian","Madurese","Magahi","Marshall",
  "Maithili","Makasar","Malayalam","Mandingo",
  "Maori","Austronesian (Other)","Marathi","Masai",
  "Manx","Malay","Mandar","Mende",
  "Irish, Middle (900-1200)","Micmac","Minangkabau","Miscellaneous languages",
  "Mon-Khmer (Other)","Malagasy","Maltese","Manipuri",
  "Manobo languages","Mohawk","Moldavian","Mongolian",
  "Mossi","Multiple languages","Munda languages","Creek",
  "Marwari","Mayan languages","Aztec","North American Indian (Other)",
  "Nauru","Navajo","Ndebele, South","Ndebele, North",
  "Ndonga","Nepali","Newari","Nias",
  "Niger-Kordofanian (Other)","Niuean","Norse, Old","Norwegian",
  "Sohto, Northern ","Nubian languages","Nyanja","Nyamwezi",
  "Nyankole","Nyoro","Nzima","Occitan (post 1500)",
  "Ojibwa","Oriya","Oromo","Osage",
  "Ossetic","Turkish, Ottoman (1500-1928)","Otomian languages","Papuan (Other)",
  "Pangasinan","Pahlavi","Pampanga","Panjabi",
  "Papiamento","Palauan","Persian, Old (ca. 600-400 B.C.)","Persian",
  "Philippine (Other)","Phoenician","Pali","Polish",
  "Pohnpeian","Portuguese","Prakrit languages","Provenc,al, Old (to 1500)",
  "Pushto","Quechua","Rajasthani","Rapanui",
  "Rarotongan","Romance (Other)","Raeto-Romance","Romany",
  "Romanian","Rundi","Russian","Sandawe",
  "Sango","South American Indian (Other)","Salishan languages","Samaritan Aramaic",
  "Sanskrit","Sasak","Santali","Serbian",
  "Scots","Croatian","Selkup","Semitic (Other)",
  "Irish, Old (to 900)","Shan","Sidamo","Sinhalese",
  "Siouan languages","Sino-Tibetan (Other)","Slavic (Other)","Slovak",
  "Slovenian","S½mi languages","Samoan","Shona",
  "Sindhi","Soninke","Sogdian","Somali",
  "Songhai","Sotho, Southern","Spanish","Sardinian",
  "Serer","Nilo-Saharan (Other)","Swati","Sukuma",
  "Sundanese","Susu","Sumerian","Swahili",
  "Swedish","Syriac","Tahitian","Tai (Other)",
  "Tamil","Tatar","Telugu","Timne",
  "Tereno","Tetum","Tajik","Tagalog",
  "Thai","Tibetan","Tigre","Tigrinya",
  "Tiv","Tokelau","Tlingit","Tamashek",
  "Tonga (Nyasa)","Tonga (Tonga Islands)","Tok Pisin","Tsimshian",
  "Tswana","Tsonga","Turkmen","Tumbuka",
  "Turkish","Altaic (Other)","Tuvalu","Twi",
  "Tuvinian","Ugaritic","Uighur","Ukrainian",
  "Umbundu","Undetermined","Urdu","Uzbek",
  "Vai","Venda","Vietnamese","VolapŸk",
  "Votic","Wakashan languages","Walamo","Waray",
  "Washo","Welsh","Sorbian languages","Wolof",
  "Xhosa","Yao","Yapese","Yiddish",
  "Yoruba","Yupik languages","Zapotec","Zenaga",
  "Zhuang","Zande","Zulu","Zu–i"
};

char *AudioType[5]={
  "Undefined",
  "Clean effects",
  "Hearing impaired",
  "Visual impaired commentary",
  "Reserved"
};

char *DescriptorTitle[22]={
  "reserved",
  "reserved",
  "video_stream_descriptor",
  "audio_stream_descriptor",
  "hierarchy_descriptor",
  "registration_descriptor",
  "data_stream_alignment_descriptor",
  "target_background_grid_descriptor",
  "video_window_descriptor",
  "CA_descriptor",
  "ISO_639_language_descriptor",
  "system_clock_descriptor",
  "multiplex_buffer_utilization_descriptor",
  "copyright_descriptor",
  "maximum_bitrate_descriptor",
  "private_data_indicator_descriptor",
  "smoothing_buffer_descriptor",
  "STD_descriptor",
  "IBP_descriptor",
  "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved",
  "User private",
  "Unknown, internal error"
};

char *FrameRate[16]={
  "Forbidden",
  "23.976",
  "24",
  "25",
  "29.97",
  "30",
  "50",
  "59.94",
  "60",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved"
};

char *ChromaFormat[4]={
  "Reserved",
  "4:2:0",
  "4:2:2",
  "4:4:4"
};

char *Profile[8]={
  "Reserved",
  "High",
  "Spatially Scalable",
  "SNR Scalable",
  "Main",
  "Simple",
  "Reserved",
  "Reserved"
};

char *Level[16]={
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "High",
  "Reserved",
  "High 1440",
  "Reserved",
  "Main",
  "Reserved",
  "Low",
  "Reserved",
  "Reserved"
  "Reserved",
  "Reserved",
  "Reserved"
};

char *AudioLayer[4]={
  "I",
  "II",
  "III",
  "Forbidden"
};

char *HierarchyType[16]={
  "Reserved",
  "ITU-T Rec. H.262 | ISO/IEC 13818-2 Spatial Scalability",
  "ITU-T Rec. H.262 | ISO/IEC 13818-2 SNR Scalability",
  "ITU-T Rec. H.262 | ISO/IEC 13818-2 Temporal Scalability",
  "ITU-T Rec. H.262 | ISO/IEC 13818-2 Data partitioning",
  "ISO/IEC 13818-3 Extension bitstream",
  "ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Private Stream",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved"
  "Reserved",
  "Reserved",
  "Base Layer"
};

void show_descriptor(Descriptor *descriptor, int verbose) {
  
  unsigned char tag;
  int i,j,n,length;
  unsigned char *data;
  
  char *str;
  str=(char *)malloc(1000);
  
  if (descriptor==NULL) {
    if (verbose) printf("#Descriptor: uninitialized\n");
    return;
  }
  tag=descriptor->tag;
  length=descriptor->length;
  data=descriptor->data;

  if (verbose) {
    printf("#Descriptor: ");
    if (tag>=64) printf("%s",DescriptorTitle[20]);
    else if (tag>=19) printf("%s",DescriptorTitle[19]);
    else printf("%s",DescriptorTitle[tag]);
    printf(" (0x%02X) %i bytes",tag,length);
    if ((length>0) && (data==NULL)) {
      printf("\n#  ERROR: Data block uninitialized!\n");
      return;
    }
    for (i=0; i<length; i++) {
      if (!(i % 24)) printf("\n#  ");
      printf("%02X ",(data[i] & 0xFF));
    }
    printf("\n");
  } else if ((length>0) && (data==NULL)) return;

  if ((tag==0x00) || (tag==0x01)) {
    if (!verbose) printf("Reserved descriptor\n");
  } else if (tag==0x02) {
    if (verbose) {
    } else {
      if (data[0]&0x04) str="";
      else {
        sprintf(str,", Profile: %s%s%s %s FRX:%c",
          ((data[1]&0x80)?"Reserved":Profile[(data[1]>>4)&0x07]),
          ((data[1]&0x80)?"":" / "),
          ((data[1]&0x80)?"":Level[data[1]&0x0F]),
          ChromaFormat[(data[2]>>6)&0x03],
          ((data[2]&0x20)?'1':'0'));
      }
      printf("Video: %sFrameRate:%s, MPEG-%c, %sconstrained %s pictures%s\n",
        ((data[0]&0x80)?"Multiple":""),
        FrameRate[(data[0]>>3)&0x0F],
        ((data[0]&0x04)?'1':'2'),
        ((data[0]&0x02)?"":"un"),
        ((data[0]&0x01)?"still":"moving"),
        str);
    }
  } else if (tag==0x03) {
    if (verbose) {
    } else {
      printf("Audio: MPEG-1 Layer %s, FreeFormat: %c, ID: %c, VariableRate: %c\n",
        AudioLayer[((data[0]>>4)&0x03)],
        ((data[0]&0x80)?'1':'0'),
        ((data[0]&0x40)?'1':'0'),
        ((data[0]&0x08)?'1':'0'));
    }
  } else if (tag==0x04) {
    if (verbose) {
    } else {
      printf("Hierarchy: %s, LayerIndex: %i/%i, Channel: %i\n",
        HierarchyType[data[0]&0x0F],
        data[1]&0x3F,
        data[2]&0x3F,
        data[3]&0x3F);
    }
  } else if (tag==0x05) {
    if (verbose) {
    } else {
      printf("Registration: Format identifier 0x%08X\n",
        (((((((data[0]<<8)|(data[1]&0xFF))<<8)|(data[2]&0xFF))<<8)|(data[3]&0xFF))<<8)
      );	
      n=length-4;
      for (i=0; i<n/16; i++) {
        for (j=0; (j<16) && (j+i*16<n); j++) {
          printf(" %02X",data[4+j+i*16]);
        }
        printf("  ");
        for (j=0; (j<16) && (j+i*16<n); j++) {
          printf("%c",((data[4+j+i*16]<32)?'.':data[4+j+i*16]));
        }
        printf("\n");
      }
    }
  } else if (tag==0x06) {
    if (verbose) {
    } else {
      printf("Data Stream Alignment:\n");
    }
  } else if (tag==0x07) {
    if (verbose) {
    } else {
      printf("Target Background Grid:\n");
    }
  } else if (tag==0x08) {
    if (verbose) {
    } else {
      printf("Video Window:\n");
    }
  } else if (tag==0x09) {
    if (verbose) {
    } else {
      printf("Conditional Access:\n");
    }
  } else if (tag==0x0A) {
    if ((data!=NULL) && (length>=4)) {
      str[0]='\0';
      i=0;
      while ((i<432) && (iso639_2b_code[i][0]<data[0])) i++;
      if (iso639_2b_code[i][0]!=data[0]) i=432;
      while ((i<432) && (iso639_2b_code[i][1]<data[1])) i++;
      if (iso639_2b_code[i][1]!=data[1]) i=432;
      while ((i<432) && (iso639_2b_code[i][2]<data[2])) i++;
      if (iso639_2b_code[i][2]!=data[2]) i=432;
      if (i<432) {
        strcat(str,iso639_2b_name[i]);
      } else {
        i=0;
        while ((i<432) && (iso639_2t_code[i][0]<data[0])) i++;
        if (iso639_2b_code[i][0]!=data[0]) i=432;
        while ((i<432) && (iso639_2t_code[i][1]<data[1])) i++;
        if (iso639_2b_code[i][1]!=data[1]) i=432;
        while ((i<432) && (iso639_2t_code[i][2]<data[2])) i++;
        if (iso639_2b_code[i][2]!=data[2]) i=432;
        if (i<432) {
          strcat(str,iso639_2t_name[i]);
          strcat(str,"[T]");
        } else {
          i=0;
          while ((i<136) && (iso639_1_code[i][0]<data[0])) i++;
          if (iso639_1_code[i][0]!=data[0]) i=136;
          while ((i<136) && (iso639_1_code[i][1]<data[1])) i++;
          if (iso639_1_code[i][1]!=data[1]) i=136;
          if (i<136) {
            strcat(str,iso639_1_name[i]);
            strcat(str,"[1]");
          } else {
            strcat(str,"Unknown");
          }
        }
      }
      printf((verbose)?"#  Language:   %s (%c%c%c)\n#  Audio Type: %s\n":"Language: %s (%c%c%c) / %s\n",
        str,
        data[0],data[1],data[2],
        ((data[3]<=3)?AudioType[data[3]]:AudioType[4]));
    }
  } else if (tag==0x0B) {
    if (verbose) {
    } else {
      printf("System Clock:\n");
    }
  } else if (tag==0x0C) {
    if (verbose) {
    } else {
      printf("Multiplex Buffer Utilization:\n");
    }
  } else if (tag==0x0D) {
    if (verbose) {
    } else {
      printf("Copyright:\n");
    }
  } else if (tag==0x0E) {
    if (verbose) {
    } else {
      printf("Maximum Bitrate:\n");
    }
  } else if (tag==0x0F) {
    if (verbose) {
    } else {
      printf("Private Data Indicator:\n");
    }
  } else if (tag==0x10) {
    if (verbose) {
    } else {
      printf("Smoothing Buffer:\n");
    }
  } else if (tag==0x11) {
    if (verbose) {
    } else {
      printf("STD:\n");
    }
  } else if (tag==0x12) {
    if (verbose) {
    } else {
      printf("IBP:\n");
    }
  } else if (tag>=0x40) {
    if (verbose) {
    } else {
      printf("User private descriptor\n");
    }
  } else if (tag>=0x13) {
    if (verbose) {
    } else {
      printf("Reserved descriptor\n");
    }
  } else ;
}
