($a = gc .\onegin.txt) | sort >onegin_res.txt; $a | sort {-join$_[-1..-$_.Length]} >>onegin_res.txt; $a >>.\onegin_res.txt
