# algorithms_test.ino
- Control of servo logic for a 60 second "continuous testing" session. It features a time syncing signature and four different test sections (slow sweep, fast sweep, move and hold, small corrections)

# demo_videos.ino
- A shortened version meant to run only one test section at a time for the purpose of video analysis. No sync signature is included

### algorithms_test.ino timing and structure: After pressing button one, the following sequence takes place:
<0 → PRE	
<2000 → SYNC	
<3000 → SYNC PAUSE	
<4000 → MOVE TO START	
<4500 → PAUSE BEFORE SEG1	
<19500 → SECTION 1	
<20000 → PAUSE AFTER SEG1	
<30000 → SECTION 2	
<30500 → PAUSE AFTER SEG2	
<31000 → TRANSITION TO SEG3	
<31500 → PAUSE BEFORE SEG3	
<46500 → SECTION 3	
<47000 → PAUSE AFTER SEG3	
<55000 → SECTION 4	
<55500 → PAUSE AFTER SEG4	
<56000 → RETURN TO START	
This fits into the 60 second "continuous testing" VisAR command if started within the first few seconds.
