set term pdf
set output "./hexagonal-topology.gnuplot.pdf"
set style arrow 1 lc "black" lt 1 head filled
set xrange [-6929:6929]
set yrange [-6929:6929]
set arrow 1 from 0,0 rto 124.996,72.1667 arrowstyle 1 
set object 1 polygon from \
-500.851, 1443.83 to \
-1000.84, 1732.5 to \
-1000.84, 2309.83 to \
-500.851, 2598.5 to \
-0.866025, 2309.83 to \
-0.866025, 1732.5 to \
-500.851, 1443.83 front fs empty 
set label 1 "1" at -500.851 , 2021.17 center
set arrow 2 from 0,0 rto -124.996,72.1667 arrowstyle 1 
set object 2 polygon from \
1.05871e-13, 576.333 to \
-499.985, 865 to \
-499.985, 1442.33 to \
1.05871e-13, 1731 to \
499.985, 1442.33 to \
499.985, 865 to \
1.05871e-13, 576.333 front fs empty 
set label 2 "2" at 1.05871e-13 , 1153.67 center
set arrow 3 from 0,0 rto -2.65136e-14,-144.333 arrowstyle 1 
set object 3 polygon from \
-999.105, 577.833 to \
-1499.09, 866.5 to \
-1499.09, 1443.83 to \
-999.105, 1732.5 to \
-499.119, 1443.83 to \
-499.119, 866.5 to \
-999.105, 577.833 front fs empty 
set label 3 "3" at -999.105 , 1155.17 center
set arrow 4 from 1499.96,866 rto 124.996,72.1667 arrowstyle 1 
set object 4 polygon from \
-2000.81, 577.833 to \
-2500.79, 866.5 to \
-2500.79, 1443.83 to \
-2000.81, 1732.5 to \
-1500.82, 1443.83 to \
-1500.82, 866.5 to \
-2000.81, 577.833 front fs empty 
set label 4 "4" at -2000.81 , 1155.17 center
set arrow 5 from 1499.96,866 rto -124.996,72.1667 arrowstyle 1 
set object 5 polygon from \
-1499.96, -289.667 to \
-1999.94, -1 to \
-1999.94, 576.333 to \
-1499.96, 865 to \
-999.971, 576.333 to \
-999.971, -1 to \
-1499.96, -289.667 front fs empty 
set label 5 "5" at -1499.96 , 287.667 center
set arrow 6 from 1499.96,866 rto -2.65136e-14,-144.333 arrowstyle 1 
set object 6 polygon from \
-999.105, -1154.17 to \
-1499.09, -865.5 to \
-1499.09, -288.167 to \
-999.105, 0.5 to \
-499.119, -288.167 to \
-499.119, -865.5 to \
-999.105, -1154.17 front fs empty 
set label 6 "6" at -999.105 , -576.833 center
set arrow 7 from 1.06054e-13,1732 rto 124.996,72.1667 arrowstyle 1 
set object 7 polygon from \
-2000.81, -1154.17 to \
-2500.79, -865.5 to \
-2500.79, -288.167 to \
-2000.81, 0.5 to \
-1500.82, -288.167 to \
-1500.82, -865.5 to \
-2000.81, -1154.17 front fs empty 
set label 7 "7" at -2000.81 , -576.833 center
set arrow 8 from 1.06054e-13,1732 rto -124.996,72.1667 arrowstyle 1 
set object 8 polygon from \
-1499.96, -2021.67 to \
-1999.94, -1733 to \
-1999.94, -1155.67 to \
-1499.96, -867 to \
-999.971, -1155.67 to \
-999.971, -1733 to \
-1499.96, -2021.67 front fs empty 
set label 8 "8" at -1499.96 , -1444.33 center
set arrow 9 from 1.06054e-13,1732 rto -2.65136e-14,-144.333 arrowstyle 1 
set object 9 polygon from \
500.851, -2020.17 to \
0.866025, -1731.5 to \
0.866025, -1154.17 to \
500.851, -865.5 to \
1000.84, -1154.17 to \
1000.84, -1731.5 to \
500.851, -2020.17 front fs empty 
set label 9 "9" at 500.851 , -1442.83 center
set arrow 10 from -1499.96,866 rto 124.996,72.1667 arrowstyle 1 
set object 10 polygon from \
-500.851, -2020.17 to \
-1000.84, -1731.5 to \
-1000.84, -1154.17 to \
-500.851, -865.5 to \
-0.866025, -1154.17 to \
-0.866025, -1731.5 to \
-500.851, -2020.17 front fs empty 
set label 10 "10" at -500.851 , -1442.83 center
set arrow 11 from -1499.96,866 rto -124.996,72.1667 arrowstyle 1 
set object 11 polygon from \
-3.18347e-13, -2887.67 to \
-499.985, -2599 to \
-499.985, -2021.67 to \
-3.18347e-13, -1733 to \
499.985, -2021.67 to \
499.985, -2599 to \
-3.18347e-13, -2887.67 front fs empty 
set label 11 "11" at -3.18347e-13 , -2310.33 center
set arrow 12 from -1499.96,866 rto -2.65136e-14,-144.333 arrowstyle 1 
set object 12 polygon from \
2000.81, -1154.17 to \
1500.82, -865.5 to \
1500.82, -288.167 to \
2000.81, 0.5 to \
2500.79, -288.167 to \
2500.79, -865.5 to \
2000.81, -1154.17 front fs empty 
set label 12 "12" at 2000.81 , -576.833 center
set arrow 13 from -1499.96,-866 rto 124.996,72.1667 arrowstyle 1 
set object 13 polygon from \
999.105, -1154.17 to \
499.119, -865.5 to \
499.119, -288.167 to \
999.105, 0.5 to \
1499.09, -288.167 to \
1499.09, -865.5 to \
999.105, -1154.17 front fs empty 
set label 13 "13" at 999.105 , -576.833 center
set arrow 14 from -1499.96,-866 rto -124.996,72.1667 arrowstyle 1 
set object 14 polygon from \
1499.96, -2021.67 to \
999.971, -1733 to \
999.971, -1155.67 to \
1499.96, -867 to \
1999.94, -1155.67 to \
1999.94, -1733 to \
1499.96, -2021.67 front fs empty 
set label 14 "14" at 1499.96 , -1444.33 center
set arrow 15 from -1499.96,-866 rto -2.65136e-14,-144.333 arrowstyle 1 
set object 15 polygon from \
500.851, -288.167 to \
0.866025, 0.5 to \
0.866025, 577.833 to \
500.851, 866.5 to \
1000.84, 577.833 to \
1000.84, 0.5 to \
500.851, -288.167 front fs empty 
set label 15 "15" at 500.851 , 289.167 center
set arrow 16 from -3.18163e-13,-1732 rto 124.996,72.1667 arrowstyle 1 
set object 16 polygon from \
-500.851, -288.167 to \
-1000.84, 0.5 to \
-1000.84, 577.833 to \
-500.851, 866.5 to \
-0.866025, 577.833 to \
-0.866025, 0.5 to \
-500.851, -288.167 front fs empty 
set label 16 "16" at -500.851 , 289.167 center
set arrow 17 from -3.18163e-13,-1732 rto -124.996,72.1667 arrowstyle 1 
set object 17 polygon from \
-1.83697e-16, -1155.67 to \
-499.985, -867 to \
-499.985, -289.667 to \
-1.83697e-16, -1 to \
499.985, -289.667 to \
499.985, -867 to \
-1.83697e-16, -1155.67 front fs empty 
set label 17 "17" at -1.83697e-16 , -578.333 center
set arrow 18 from -3.18163e-13,-1732 rto -2.65136e-14,-144.333 arrowstyle 1 
set object 18 polygon from \
2000.81, 577.833 to \
1500.82, 866.5 to \
1500.82, 1443.83 to \
2000.81, 1732.5 to \
2500.79, 1443.83 to \
2500.79, 866.5 to \
2000.81, 577.833 front fs empty 
set label 18 "18" at 2000.81 , 1155.17 center
set arrow 19 from 1499.96,-866 rto 124.996,72.1667 arrowstyle 1 
set object 19 polygon from \
999.105, 577.833 to \
499.119, 866.5 to \
499.119, 1443.83 to \
999.105, 1732.5 to \
1499.09, 1443.83 to \
1499.09, 866.5 to \
999.105, 577.833 front fs empty 
set label 19 "19" at 999.105 , 1155.17 center
set arrow 20 from 1499.96,-866 rto -124.996,72.1667 arrowstyle 1 
set object 20 polygon from \
1499.96, -289.667 to \
999.971, -1 to \
999.971, 576.333 to \
1499.96, 865 to \
1999.94, 576.333 to \
1999.94, -1 to \
1499.96, -289.667 front fs empty 
set label 20 "20" at 1499.96 , 287.667 center
set arrow 21 from 1499.96,-866 rto -2.65136e-14,-144.333 arrowstyle 1 
set object 21 polygon from \
500.851, 1443.83 to \
0.866025, 1732.5 to \
0.866025, 2309.83 to \
500.851, 2598.5 to \
1000.84, 2309.83 to \
1000.84, 1732.5 to \
500.851, 1443.83 front fs empty 
set label 21 "21" at 500.851 , 2021.17 center
set label at 724.094 , 598.979 point pointtype 7 pointsize 0.2 center
set label at -120.625 , 409.495 point pointtype 7 pointsize 0.2 center
set label at 283.491 , -470.452 point pointtype 7 pointsize 0.2 center
set label at 2218.74 , 1286.57 point pointtype 7 pointsize 0.2 center
set label at 1330.75 , 1208.11 point pointtype 7 pointsize 0.2 center
set label at 1934.81 , 186.879 point pointtype 7 pointsize 0.2 center
set label at 190.065 , 2236.14 point pointtype 7 pointsize 0.2 center
unset key
plot 1/0
