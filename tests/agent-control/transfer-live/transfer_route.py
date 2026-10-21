"""Zero-storage destination refusal only; operator-created two-base campaign."""
from pathlib import Path
import re
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'research-live'))
from route import Route, RouteError, amount, require

class TransferRoute(Route):
    def __init__(self,*args,source_name='MCP Research',destination_name='NEWBASE',item='Pistol',**kwargs):
        super().__init__(*args,**kwargs)
        require(source_name!=destination_name,'base_names_must_differ')
        self.source_name=source_name;self.destination_name=destination_name;self.item=item

    def select_named_base(self,name):
        state=self.ready({'basescape'})
        require(len(state['visible'].get('base_slots',[]))==2,'exactly_two_visible_bases_required')
        if state['visible'].get('base_name')==name:return state
        for index in range(2):
            def target(s):
                slots=s['visible'].get('base_slots',[])
                require(len(slots)==2,'base_slots_changed')
                return {'target':slots[index]['handle']}
            state=self.act({'basescape'},'select_base',target,{'basescape'})
            if state['visible'].get('base_name')==name:return state
        raise RouteError('named_base_not_observed:'+name)

    def stores_item(self):
        previous=None;unknown=False;found=None
        for page in range(41):
            state=self.ready({'base_stores'});v=state['visible'];rows=v.get('rows')
            require(isinstance(rows,list),'stores_rows_unknown')
            window=[];names=[]
            for row in rows:
                cells=row.get('cells');require(isinstance(cells,list) and len(cells)==3,'stores_cells_unknown')
                window.append(cells)
                if not isinstance(cells[0],str) or not cells[0]:unknown=True;continue
                names.append(cells[0])
                if cells[0]==self.item:
                    quantity=amount(cells[1]);size=cells[2]
                    require(isinstance(size,str) and re.fullmatch(r'[0-9]+(?:\.[0-9]+)?',size),'item_size_unknown')
                    value=(quantity,float(size))
                    require(found is None or found==value,'stores_item_changed_during_scan');found=value
            require(len(names)==len(set(names)),'ambiguous_stores_names')
            self.record('stores_page',item=self.item,page=page,rows=window)
            if window==previous or v.get('scroll',{}).get('down') is False:
                require(found is not None or not unknown,'unknown_labels_prevent_zero_baseline')
                return found or (0,0.0)
            require(rows and page<40 and v.get('scroll',{}).get('down') is True,'stores_scan_incomplete')
            previous=window
            self.act({'base_stores'},'scroll_visible_list',{'direction':'down'},{'base_stores'})
        raise RouteError('stores_scan_incomplete')

    def snapshot(self,name):
        self.select_named_base(name)
        self.act({'basescape'},'open_base_info',after={'base_info'},guard=lambda s:require(s['visible'].get('base_name')==name,'base_changed'))
        state=self.ready({'base_info'});v=state['visible']
        require(v.get('base_name')==name,'base_info_identity_unknown')
        stores=[m for m in v.get('metrics',[]) if m.get('label')=='Stores']
        require(len(stores)==1 and isinstance(stores[0].get('value'),str),'stores_capacity_unknown')
        match=re.fullmatch(r'([0-9]+):([0-9]+)',stores[0]['value'])
        require(match is not None,'stores_capacity_unparseable')
        capacity=tuple(map(int,match.groups()))
        self.act({'base_info'},'open_base_stores',after={'base_stores'})
        quantity,size=self.stores_item()
        self.act({'base_stores'},'close_base_stores',after={'base_info'})
        self.act({'base_info'},'close_base_info',after={'basescape'})
        result={'base_name':name,'displayed_used_capacity':capacity,'quantity':quantity,'displayed_total_item_size':size}
        self.record('base_snapshot',**result)
        return result

    def transfer_row(self,state):
        rows=[r for r in state['visible'].get('rows',[]) if r.get('cells') and r['cells'][0]==self.item]
        require(len(rows)==1,'unique_transfer_item_required')
        row=rows[0];require(len(row['cells'])==4,'transfer_column_count')
        return row

    def locate_transfer_item(self):
        previous=None
        for page in range(33):
            state=self.ready({'transfer_items'});rows=state['visible'].get('rows',[])
            matches=[r for r in rows if r.get('cells') and r['cells'][0]==self.item]
            if matches:return self.transfer_row(state)
            window=[r.get('cells') for r in rows]
            require(window!=previous and page<32 and state['visible'].get('scroll',{}).get('down') is True,'transfer_item_not_visible')
            previous=window
            self.act({'transfer_items'},'scroll_visible_list',{'direction':'down'},{'transfer_items'})
        raise RouteError('transfer_item_not_visible')

    def run(self):
        self.act({'geoscape'},'open_bases',after={'basescape'})
        source=self.snapshot(self.source_name);dest=self.snapshot(self.destination_name)
        require(source['quantity']>=1 and source['displayed_total_item_size']>0,'positive_sized_source_item_required')
        require(dest['displayed_used_capacity']==(0,0) and dest['quantity']==0,'destination_must_have_zero_storage')
        self.select_named_base(self.source_name)
        self.act({'basescape'},'open_transfer',after={'transfer_destination'})
        def destination(s):
            rows=[r for r in s['visible'].get('rows',[]) if r.get('cells') and r['cells'][0]==self.destination_name]
            require(len(rows)==1,'unique_destination_label_required');return {'row':rows[0]['handle']}
        self.act({'transfer_destination'},'choose_visible_row',destination,{'transfer_items'})
        self.locate_transfer_item()
        def request(s):
            row=self.transfer_row(s)
            require([amount(c) for c in row['cells'][1:]]==[source['quantity'],0,0],'transfer_baseline_mismatch')
            require('increase' in row.get('controls',[]),'transfer_increase_not_offered')
            return {'row':row['handle'],'operation':'adjust','value':1}
        self.act({'transfer_items'},'change_transfer_quantity',request,{'error_message'})
        state=self.ready({'error_message'});v=state['visible']
        require(v.get('heading')=='NOT ENOUGH STORE SPACE!' or (isinstance(v.get('message'),str) and v['message'].startswith('NOT ENOUGH STORE SPACE!')),'wrong_or_unknown_capacity_refusal')
        self.record('zero_storage_refusal',visible=v,full_or_overfull_coverage=False)
        self.act({'error_message'},'dismiss_error',after={'transfer_items'})
        row=self.transfer_row(self.ready({'transfer_items'}))
        require([amount(c) for c in row['cells'][1:]]==[source['quantity'],0,0],'refusal_changed_draft')
        self.act({'transfer_items'},'cancel_transfer_items',after={'basescape'})
        after_source=self.snapshot(self.source_name);after_dest=self.snapshot(self.destination_name)
        require(after_source==source and after_dest==dest,'cancel_changed_visible_inventory_or_capacity')
        self.select_named_base(self.source_name)
        self.act({'basescape'},'return_geoscape')
        self.act({'geoscape'},'open_pause_menu',after={'pause_menu'})
        self.record('zero_storage_transfer_pass',dispatches=self.count,source=source,destination=dest,
                    scope='zero-capacity destination refusal and cancel only',shipment_committed=False)
