# Signing (manual)

Manual signing is recommended for all FW iamges that will be released to customers.

### Generate the root key

```
$ tools/sbl.py keygen --split=3,5
split 1
  key: 1:az5eo-zfcqr-nx836-w3o65-crg9b-qd68f-9d7qr-pfc1q-43xmb-ouo6w-7moui-1ezas-51byn-8pnts-fcom5-isrd9-a9tza-ujftx-nod56-uhrx4-xqxxj-oc3hm-6fgdy-px136-m5tzh-8ugj5-kh6e5-zhbor-mehho-wkdsz-wwt5w-3jaas-qxeke-fd
  pass phrase: rampantly-awoke-neither-liquid-vicinity-shovel-eulogy-litter
split 2
  key: 2:17xpw-m9qjb-gniee-ia18c-ptuoj-ouiwr-e6bxw-rxsk5-dbie4-4ydb8-3is8p-emkui-rkciq-8tue8-g13ek-zdiui-7optt-4yru3-ifcdo-pwerx-j83o6-aw6un-gfr65-zo996-zy3yo-smsgz-mmwq1-tnius-neewh-zxqmt-75kk5-x6upw-wjwie-8y
  pass phrase: taco-sixfold-illuminate-lullaby-siesta-magazine-doorstep-wristwatch
split 3
  key: 3:oepne-mqob4-hz9qp-wgs76-3zm43-35k76-94htf-o1dad-hh9qk-3ws45-oen7y-7q93h-ojqhg-jdm9s-bht3h-78hmi-yf3wq-uy57b-cb3hm-kxz3r-8y7c8-bypaf-47mtk-6hjzq-snase-6pexp-wbipp-8fcpu-9oc3m-ud9af-5w18m-n6qwi-nc3q8-bd
  pass phrase: deliverer-feedback-luridness-crewmember-videogame-gerbil-yogurt-abiding
split 4
  key: 4:ap5zi-w96ub-xatac-dzak4-hqhwi-58qj1-e81j9-hsxbi-p69uz-gm1fu-zrcrq-6ouq6-w7are-qf6ph-9wy6h-ifmrh-1utk3-cxeib-6wkag-d8ae5-6d77e-3djpo-4usso-w63fq-fucsd-yofb4-sbd7r-rj6q7-xu9zw-p4ipg-hws6b-jz7fj-q18qs-ay
  pass phrase: rupture-amplifier-marbles-spur-tiara-dibs-halogen-briefcase
split 5
  key: 5:e46k1-qe9xs-bihby-76e5j-9paig-sqacf-tbckj-u93kx-6req7-8z569-ahooi-d4rx9-xrr7s-q36cq-mhas4-5kzro-n4c4a-zbxeu-hmnig-ui14k-wrryu-k6rju-autt7-p9dc9-3sgdd-8wjez-8peu3-186um-m3x9p-6cpbb-k149r-3yw68-ggtxw-4y
  pass phrase: pucker-arachnid-request-flashlight-maestro-railroad-tutorial-enhance
public key:
  6015f368b974ba14f37072a1fff89c66b1e84609c8187d90a59b60ca7f1c263f
```

* The `key` portion of each split should be copied to a different USB stick and stored in physically different locations assessable by the key split owner.
* The `pass phrase` portion of each split should be saved in each key split owner's  password manager (e.g. 1password, or something similar).
* The `public key` portion can be copied to a file and made available to any process that needs it.


### Generate the primary key

```
$ tools/sbl.py keygen --split=3,5
split 1
  key: 1:yoxjk-qnqgo-63khk-4yr97-rfriz-ijxms-nqqfb-naq8h-pzjau-caw6t-7fdna-oyomk-iajy9-nxbmf-okf3e-4fy6e-snc3o-nf9e7-jhgn7-j35pz-xpsrc-9drcy-3sjnp-pcb3x-8bg53-3511f-kyzk6-zpn15-ya837-rjx74-dd9fx-8sybm-kea4s-bd
  pass phrase: cabin-gosling-asleep-abhorrence-pantyhose-leukemia-zookeeper-folksong
split 2
  key: 2:qib4n-dextq-myx7z-u8td9-f6t8d-qpgyh-aaw3y-njxgw-w811c-moj6g-swh41-jq8pa-1zak1-5cx64-uj78j-hhtny-3otqe-8gk7j-fb16q-hogc1-7op1k-cmks5-8g33a-4ydn5-gupas-9xexw-eycc1-x1o4o-g66xq-w7dxi-j8baa-fumer-ftgqb-4b
  pass phrase: yard-hardhat-clipboard-family-kimono-daytime-explode-jawbreaker
split 3
  key: 3:ujgo5-xz55w-cu9b8-8yssd-b9n6q-ui3ir-8zs11-fisza-z46rz-w547s-fw3cj-s76c8-bgkxq-nckpr-ftx7a-a97zs-bpb4r-11pwp-tw7u7-unntk-f83bm-fw6ir-eh98u-9ofck-9psc6-nyw3n-grmts-is7rt-6ta4y-9yku4-h9i8d-jgze3-8z181-hb
  pass phrase: nullify-gearbox-festival-schoolbook-lettuce-asleep-enhance-luscious
split 4
  key: 4:hxhxb-jowti-ga5xd-hznkr-fz9mn-69hsx-y6yeh-asisg-yur3o-3aenz-rzo6p-ix9kw-uhoij-i3s5y-fbxe8-gzcgg-dup7c-rhygg-rdecy-y9p1i-jor9u-ekat7-1wp6r-oh8sg-jd1h5-6t7pn-bd3e9-g9hf1-3b6wp-zob7m-snank-syog9-9wuxq-pn
  pass phrase: rubber-unpaved-rudderless-kilogram-library-chrome-eagerness-exhale
split 5
  key: 5:4grqe-7xshi-ruzwb-5a791-6efd5-nztr3-eyg4o-3fwso-5n4wg-nc9rc-5o8cn-d5qc6-ui8er-pnz7y-61dhy-p6pu4-hpead-g9xjp-ourgm-7ee1h-fgpcr-hkyer-ag9ko-h49w1-j59or-zyxm7-edonk-3tpbk-p9qur-op9rx-beeoo-pfzmq-exfgn-hd
  pass phrase: anchor-reentry-fondue-emancipate-widow-emoticon-enjoyably-debris
public key:
  3a3eca181ba046bc404e97bbd7a8ce547b0b09dbf1b963b8696f139c041a4678
```

* The `key` portion of each split should be copied to a different USB stick and stored in physically different locations assessable by the key split owner.
* The `pass phrase` portion of each split should be saved in each key split owner's  password manager (e.g. 1password, or something similar).
* The `public key` portion can be copied to a file and made available to any process that needs it.

### Generate a certificate for the primary key

```
$ tools/sbl.py certgen --pub 3a3eca181ba046bc404e97bbd7a8ce547b0b09dbf1b963b8696f139c041a4678 primary.cert
first key: 1:az5eo-zfcqr-nx836-w3o65-crg9b-qd68f-9d7qr-pfc1q-43xmb-ouo6w-7moui-1ezas-51byn-8pnts-fcom5-isrd9-a9tza-ujftx-nod56-uhrx4-xqxxj-oc3hm-6fgdy-px136-m5tzh-8ugj5-kh6e5-zhbor-mehho-wkdsz-wwt5w-3jaas-qxeke-fd
passphase 1: rampantly-awoke-neither-liquid-vicinity-shovel-eulogy-litter
next key: 2:17xpw-m9qjb-gniee-ia18c-ptuoj-ouiwr-e6bxw-rxsk5-dbie4-4ydb8-3is8p-emkui-rkciq-8tue8-g13ek-zdiui-7optt-4yru3-ifcdo-pwerx-j83o6-aw6un-gfr65-zo996-zy3yo-smsgz-mmwq1-tnius-neewh-zxqmt-75kk5-x6upw-wjwie-8y
passphase 2: taco-sixfold-illuminate-lullaby-siesta-magazine-doorstep-wristwatch
next key: 5:e46k1-qe9xs-bihby-76e5j-9paig-sqacf-tbckj-u93kx-6req7-8z569-ahooi-d4rx9-xrr7s-q36cq-mhas4-5kzro-n4c4a-zbxeu-hmnig-ui14k-wrryu-k6rju-autt7-p9dc9-3sgdd-8wjez-8peu3-186um-m3x9p-6cpbb-k149r-3yw68-ggtxw-4y
passphase 5: pucker-arachnid-request-flashlight-maestro-railroad-tutorial-enhance
next key:
$ cat primary.cert
3499643c23cd6a7fbc39a37b913e92ab91023908f80dcdcbcea35cd8644e66cd48085c01fecc9d1ea8d2852ac3a3210331344d105621121fddf211266d25000c5a31d867000000003a3eca181ba046bc404e97bbd7a8ce547b0b09dbf1b963b8696f139c041a4678
```

### Generate the secondary key

```
$ tools/sbl.py keygen --split=3,5
split 1
  key: 1:14pom-wabeq-uypj6-5myui-5oyo5-krpmw-39onr-85bfm-z97cf-yspco-1aek1-j4cnx-zwd9r-rkqyu-chkqc-3zjub-i3xer-d8nfh-puur8-13xqm-ei4my-ogaww-m8d31-4b1xw-dpg6q-xnish-gnqos-rh83a-wa51e-45gp4-htf9k-smm5s-cerwi-qy
  pass phrase: buddhist-gnomish-pry-chute-yield-acuteness-eternal-popcorn
split 2
  key: 2:jyrkw-cuc1j-5n3ja-1mikx-m33th-tdinx-o3j6i-iiwgr-tdt5i-ucid9-qj5yk-cy7r1-zm9ym-oa1cp-gqpyd-zuxky-1yaup-9fjei-in6kj-ya31s-yt6id-89xqc-1ycbp-3cke4-fwjqt-5ht7z-fk7ax-u8dru-kqey6-eia33-qq616-xf89r-ok5m1-7y
  pass phrase: banana-dehydrator-fence-phonebook-wikipedia-rulebook-turtle-dozed
split 3
  key: 3:trc18-wb4zd-1qfaw-kr4mu-9opd9-u8fiw-prmm1-7ayau-3wabc-qs764-6kjq5-szar7-sxiqw-4bmb4-sbe38-aq3j9-fb4w8-gy73u-xax67-g4mqj-x44ca-t6ac8-ra38g-8q6da-8wzrq-ercfe-s8rwt-uihf4-5f93e-pn5j5-38rju-sfsdw-fk4yh-an
  pass phrase: pebble-elusive-ghoulishly-yield-announcer-nuptials-eulogy-glove
split 4
  key: 4:mrp4k-d5anf-xd8mw-48ie3-and37-e1eb6-i3oqk-m16qx-xd5d7-guqfs-ymm7y-oenrr-txyyt-6u49j-aygmh-upygw-9kxkc-adg99-3spwj-xntua-4m4pi-7mpp1-3tjcq-stsdm-wzicn-asm95-a1119-94rfq-o7jbh-neeac-ts4jt-aaujb-aa1gq-dy
  pass phrase: rustproof-robe-entryway-ajar-renovator-anyplace-lasagna-acetone
split 5
  key: 5:8967q-e3ry7-5kwx7-38as4-8w6d8-cis9u-boeuy-yqy53-1yie6-gkayb-1zhtm-dtac9-zprgf-ypbhi-8iphd-43m7i-t8u7e-k3hq7-ksd9q-cxh93-8448h-jiqzj-jt1i9-j31h4-ouzce-bo68n-cw1ru-3ig1w-8ycue-4eux5-5rurx-d43sa-wz8np-5n
  pass phrase: pamphlet-sisterhood-issueless-eulogy-yahoo-unjustly-enrollment-dwindling
public key:
  19aafc1e24cb34db72109530d5fcdc2654f1d8ec36897eff5f7f0de750cc77ad
```

* The "key" portion of each split should be copied to file stored on the the key split owner's computer or a USB stick in their possession.
* The "pass phrase" portion of each split should be saved in each key split owner's  password manager (e.g. 1password, or something similar).
* The "public key" portion can be copied to a file and made available to any process that needs it.


### Generate a certificate for the secondary key

```
$ tools/sbl.py certgen --pub 19aafc1e24cb34db72109530d5fcdc2654f1d8ec36897eff5f7f0de750cc77ad --chain primary.cert secondary.cert
first key: 2:qib4n-dextq-myx7z-u8td9-f6t8d-qpgyh-aaw3y-njxgw-w811c-moj6g-swh41-jq8pa-1zak1-5cx64-uj78j-hhtny-3otqe-8gk7j-fb16q-hogc1-7op1k-cmks5-8g33a-4ydn5-gupas-9xexw-eycc1-x1o4o-g66xq-w7dxi-j8baa-fumer-ftgqb-4b
passphase 2: yard-hardhat-clipboard-family-kimono-daytime-explode-jawbreaker
next key: 4:hxhxb-jowti-ga5xd-hznkr-fz9mn-69hsx-y6yeh-asisg-yur3o-3aenz-rzo6p-ix9kw-uhoij-i3s5y-fbxe8-gzcgg-dup7c-rhygg-rdecy-y9p1i-jor9u-ekat7-1wp6r-oh8sg-jd1h5-6t7pn-bd3e9-g9hf1-3b6wp-zob7m-snank-syog9-9wuxq-pn
passphase 4: rubber-unpaved-rudderless-kilogram-library-chrome-eagerness-exhale
next key: 1:yoxjk-qnqgo-63khk-4yr97-rfriz-ijxms-nqqfb-naq8h-pzjau-caw6t-7fdna-oyomk-iajy9-nxbmf-okf3e-4fy6e-snc3o-nf9e7-jhgn7-j35pz-xpsrc-9drcy-3sjnp-pcb3x-8bg53-3511f-kyzk6-zpn15-ya837-rjx74-dd9fx-8sybm-kea4s-bd
passphase 1: cabin-gosling-asleep-abhorrence-pantyhose-leukemia-zookeeper-folksong
next key:
$ cat secondary.cert
8d4474eace5ac015301928846d78fd3cde4f0a3cc6c90e1e185a9b14867cc59beeac51715826a54169320374a910a91ff2ec84bed6fe8ecca748d52703a30000cc32d8670000000019aafc1e24cb34db72109530d5fcdc2654f1d8ec36897eff5f7f0de750cc77ad3499643c23cd6a7fbc39a37b913e92ab91023908f80dcdcbcea35cd8644e66cd48085c01fecc9d1ea8d2852ac3a3210331344d105621121fddf211266d25000c5a31d867000000003a3eca181ba046bc404e97bbd7a8ce547b0b09dbf1b963b8696f139c041a4678
```

### Configure (and verify) SBL

It is recommended to make a copy the SBL binary before configuring, as you can only configure an SBL binary once.

```
$ tools/sbl.py config --root 6015f368b974ba14f37072a1fff89c66b1e84609c8187d90a59b60ca7f1c263f sbl.hex
$ tools/sbl.py config --root 6015f368b974ba14f37072a1fff89c66b1e84609c8187d90a59b60ca7f1c263f --verify sbl.hex
sbl configured with:
  pk: 6015f368b974ba14f37072a1fff89c66b1e84609c8187d90a59b60ca7f1c263f
  bl-len:         32768
  bl-state:       0x00008000
  bl-state-len:   8192
  manu-data:      0x0000a000
  manu-data-len:  4096
  slot0:          0x0000b000
  slot1:          0x00083000
  slot-len:       491520
```

### Sign (and verify) an EFI/MFI/AFI image

```
$ tools/sbl.py sign --code afi.hex --cert secondary.cert afi.signed.hex
first key: 3:trc18-wb4zd-1qfaw-kr4mu-9opd9-u8fiw-prmm1-7ayau-3wabc-qs764-6kjq5-szar7-sxiqw-4bmb4-sbe38-aq3j9-fb4w8-gy73u-xax67-g4mqj-x44ca-t6ac8-ra38g-8q6da-8wzrq-ercfe-s8rwt-uihf4-5f93e-pn5j5-38rju-sfsdw-fk4yh-an
passphase 3: pebble-elusive-ghoulishly-yield-announcer-nuptials-eulogy-glove
next key: 5:8967q-e3ry7-5kwx7-38as4-8w6d8-cis9u-boeuy-yqy53-1yie6-gkayb-1zhtm-dtac9-zprgf-ypbhi-8iphd-43m7i-t8u7e-k3hq7-ksd9q-cxh93-8448h-jiqzj-jt1i9-j31h4-ouzce-bo68n-cw1ru-3ig1w-8ycue-4eux5-5rurx-d43sa-wz8np-5n
passphase 5: pamphlet-sisterhood-issueless-eulogy-yahoo-unjustly-enrollment-dwindling
next key: 4:mrp4k-d5anf-xd8mw-48ie3-and37-e1eb6-i3oqk-m16qx-xd5d7-guqfs-ymm7y-oenrr-txyyt-6u49j-aygmh-upygw-9kxkc-adg99-3spwj-xntua-4m4pi-7mpp1-3tjcq-stsdm-wzicn-asm95-a1119-94rfq-o7jbh-neeac-ts4jt-aaujb-aa1gq-dy
passphase 5: rustproof-robe-entryway-ajar-renovator-anyplace-lasagna-acetone
next key:
$ tools/sbl.py --debug verify --root 6015f368b974ba14f37072a1fff89c66b1e84609c8187d90a59b60ca7f1c263f afi.signed.hex
info: root
  pk:       6015f368b974ba14f37072a1fff89c66b1e84609c8187d90a59b60ca7f1c263f
  date:     1970-01-01T00:00:00Z
info: primary cert valid
  pk:       3a3eca181ba046bc404e97bbd7a8ce547b0b09dbf1b963b8696f139c041a4678
  date:     2025-03-17T14:27:38Z
info: secondary cert valid
  pk:       19aafc1e24cb34db72109530d5fcdc2654f1d8ec36897eff5f7f0de750cc77ad
  date:     2025-03-17T14:33:48Z
info: image valid
  build-id: 0.6.0-21-g9a9a940-dirty, 2025-03-16T15:54:11Z, MFI
  type:     mfi
  length:   17752
  hash:     89bfc8eaca1dce5989de4bf023efba6929b65fbcb6ddf77e12c78b280f5c965024f254b0215d80bea659ac7e224daaec7507db29c70cdd306b59c0233061e2e0
  date:     2025-03-17T14:42:22Z
```

# Signing (automatic)

Automatic signing can be used for builds by developers (on their local machines) or by GitHub actions for release testing.  Once a final release binary has been tested, it can then be mianually signed for release to cusomters.

#### Generate a secondary key with a single split
For automatic development signing with GitHub actions the secondary key should be generated with a single split:

```
$ tools/sbl.py keygen --split=1,1
split 1
  key: 1:eqypi-wrgg9-hkmc4-ch64i-49nu5-6z5nj-inrj1-e4qe5-kqeo3-ewjfu-errm4-nq7ad-myr41-uboj3-1jinr-zzdp6-gk5sa-oofkt-fgpk4-si3d3-m9dpg-uau9j-5tt15-98qse-qy54b-ru5qu-u3bkk-o8af6-fyc4z-qstez-i4b3n-a6j75-jn3ad-9n
  pass phrase: dastardly-guitar-untied-lucidity-yogurt-turtle-umbrella-gangway
public key:
  de9fc55f91e053c912b59809655c6d827bc5f4afadbce27d8e1311ef1a5d8aac
```

### Save key details in a GitHub action secret
Then a product specific environment variable should be set to the combined `key` and `pass phrase` as follows (inserting a `:` between the `key` and `pass phrase`).  Set the environment variable in the GitHub web interface or for local development `export` to the environment.  For example, for a product called `XYZ`:

```
export SBL_XYZ ="1:eqypi-wrgg9-hkmc4-ch64i-49nu5-6z5nj-inrj1-e4qe5-kqeo3-ewjfu-errm4-nq7ad-myr41-uboj3-1jinr-zzdp6-gk5sa-oofkt-fgpk4-si3d3-m9dpg-uau9j-5tt15-98qse-qy54b-ru5qu-u3bkk-o8af6-fyc4z-qstez-i4b3n-a6j75-jn3ad-9n:dastardly-guitar-untied-lucidity-yogurt-turtle-umbrella-gangway"
```

### Generate a certificate for the new secondary key

```
$ tools/sbl.py certgen --pub de9fc55f91e053c912b59809655c6d827bc5f4afadbce27d8e1311ef1a5d8aac --chain primary.cert secondary-gh.cert
first key: 2:qib4n-dextq-myx7z-u8td9-f6t8d-qpgyh-aaw3y-njxgw-w811c-moj6g-swh41-jq8pa-1zak1-5cx64-uj78j-hhtny-3otqe-8gk7j-fb16q-hogc1-7op1k-cmks5-8g33a-4ydn5-gupas-9xexw-eycc1-x1o4o-g66xq-w7dxi-j8baa-fumer-ftgqb-4b
passphase 2: yard-hardhat-clipboard-family-kimono-daytime-explode-jawbreaker
next key: 4:hxhxb-jowti-ga5xd-hznkr-fz9mn-69hsx-y6yeh-asisg-yur3o-3aenz-rzo6p-ix9kw-uhoij-i3s5y-fbxe8-gzcgg-dup7c-rhygg-rdecy-y9p1i-jor9u-ekat7-1wp6r-oh8sg-jd1h5-6t7pn-bd3e9-g9hf1-3b6wp-zob7m-snank-syog9-9wuxq-pn
passphase 4: rubber-unpaved-rudderless-kilogram-library-chrome-eagerness-exhale
next key: 1:yoxjk-qnqgo-63khk-4yr97-rfriz-ijxms-nqqfb-naq8h-pzjau-caw6t-7fdna-oyomk-iajy9-nxbmf-okf3e-4fy6e-snc3o-nf9e7-jhgn7-j35pz-xpsrc-9drcy-3sjnp-pcb3x-8bg53-3511f-kyzk6-zpn15-ya837-rjx74-dd9fx-8sybm-kea4s-bd
passphase 1: cabin-gosling-asleep-abhorrence-pantyhose-leukemia-zookeeper-folksong
next key:
$ cat secondary-gh.cert
30f052b82badb6f23c6f627349bb8d69eb521e48449f0c8c03ea5d1bf448cd62fd9c40f2734844cca0b7d2c4c47c63353dcc35376f3a83e19fcb22d4081d7b06f05ad86700000000de9fc55f91e053c912b59809655c6d827bc5f4afadbce27d8e1311ef1a5d8aac3499643c23cd6a7fbc39a37b913e92ab91023908f80dcdcbcea35cd8644e66cd48085c01fecc9d1ea8d2852ac3a3210331344d105621121fddf211266d25000c5a31d867000000003a3eca181ba046bc404e97bbd7a8ce547b0b09dbf1b963b8696f139c041a4678 
```

### Sign (and verify) an EFI/MFI/AFI image

```
$ tools/sbl.py sign --key ${SBL_XYZ} --code afi.hex --cert secondary-gh.cert afi.signed.hex
$ tools/sbl.py --debug verify --root 6015f368b974ba14f37072a1fff89c66b1e84609c8187d90a59b60ca7f1c263f afi.signed.hex
info: root
  pk:       6015f368b974ba14f37072a1fff89c66b1e84609c8187d90a59b60ca7f1c263f
  date:     1970-01-01T00:00:00Z
info: primary cert valid
  pk:       3a3eca181ba046bc404e97bbd7a8ce547b0b09dbf1b963b8696f139c041a4678
  date:     2025-03-17T14:27:38Z
info: secondary cert valid
  pk:       de9fc55f91e053c912b59809655c6d827bc5f4afadbce27d8e1311ef1a5d8aac
  date:     2025-03-17T17:25:04Z
info: image valid
  build-id: 0.6.0-21-g9a9a940-dirty, 2025-03-16T15:54:11Z, MFI
  type:     mfi
  length:   17752
  hash:     89bfc8eaca1dce5989de4bf023efba6929b65fbcb6ddf77e12c78b280f5c965024f254b0215d80bea659ac7e224daaec7507db29c70cdd306b59c0233061e2e0
  date:     2025-03-17T17:33:12Z
```

# Distributing certificates and keys

* Certificates and the public portion of keys can be distributed freely.  This means they can be checked into revision control systems like Git.
* The secret portion of all keys must be controlled and have limited distribution in order to be effective.  **These must never be checked into revision control systems.**  They can be made available as GitHub secrets for automated builds using GitHub actions.




