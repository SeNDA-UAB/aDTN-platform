// for(;;){
// 	int r = rand() % nbs.num();
// 	if (strcmp(nbs.nb(r), prev_hop) != 0){
// 		add_hop(nbs.nb(r));
// 		return 0;
// 	} else if (nbs.num() == 1){
// 		return 1;
// 	}
// }

if (nbs.has_next()){
	add_hop(nbs.next());
} 
return 0;
