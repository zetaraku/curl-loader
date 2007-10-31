/*
    Design:
	use page allocator as base allocator;
	
	Identify small blocks (classification function is argument)
	and itervals;
	
	for small blocks a fixed size allocator is used;
	(fixed sizes are carved out of pages that are returned 
	by page allocator)

	for larger blocks;
	    use argument large block allocator;
	    large block allocator again uses page allocator
	    for blocks up to page size;
		and base allocator for anything larger;

	
*/		
