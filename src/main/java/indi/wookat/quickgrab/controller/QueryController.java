//
// Source code recreated from a .class file by IntelliJ IDEA
// (powered by FernFlower decompiler)
//

package indi.wookat.quickgrab.controller;

import indi.wookat.quickgrab.entity.Buyers;
import indi.wookat.quickgrab.entity.Requests;
import indi.wookat.quickgrab.entity.Results;
import indi.wookat.quickgrab.service.QueryService;
import io.swagger.v3.oas.annotations.tags.Tag;
import java.util.List;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.DeleteMapping;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

@Tag(
        name = "控制器：查询",
        description = "描述：查询控制器"
)
@RestController
public class QueryController {
    @Autowired
    private QueryService queryService;

    @GetMapping({"/getRequests"})
    public ResponseEntity<List<Requests>> getRequests(@RequestParam(required = false) String keyword, @RequestParam(required = false) String buyerId, @RequestParam(required = false) Integer type, @RequestParam(required = false) Integer status, @RequestParam(defaultValue = "id_desc") String order, @RequestParam(defaultValue = "0") int offset, @RequestParam(defaultValue = "20") int limit) {
        List<Requests> requests = this.queryService.getRequestsByFilters(keyword, buyerId, type, status, order, offset, limit);
        return ResponseEntity.ok(requests);
    }

    @GetMapping({"/getResults"})
    public ResponseEntity<List<Results>> getResults(@RequestParam(required = false) String keyword, @RequestParam(required = false) String buyerId, @RequestParam(required = false) Integer type, @RequestParam(required = false) Integer status, @RequestParam(defaultValue = "end_time_desc") String order, @RequestParam(defaultValue = "0") int offset, @RequestParam(defaultValue = "20") int limit) {
        List<Results> results = this.queryService.getResultsByFilters(keyword, buyerId, type, status, order, offset, limit);
        return ResponseEntity.ok(results);
    }

    @DeleteMapping({"/deleteRequest/{id}"})
    public ResponseEntity<Void> deleteRequest(@PathVariable int id) {
        this.queryService.deleteRequestById(id);
        return ResponseEntity.ok().build();
    }

    @DeleteMapping({"/deleteResult/{id}"})
    public ResponseEntity<Void> deleteResult(@PathVariable Long id) {
        this.queryService.deleteResultById(id);
        return ResponseEntity.ok().build();
    }

    @GetMapping({"/getResult/{id}"})
    public ResponseEntity<Results> getResult(@PathVariable Long id) {
        Results result = this.queryService.getResultsById(id);
        return ResponseEntity.ok(result);
    }

    @GetMapping({"/getBuyer"})
    public ResponseEntity<List<Buyers>> getBuyer() {
        List<Buyers> buyer = this.queryService.getAllBuyer();
        return ResponseEntity.ok(buyer);
    }
}
